#!/bin/bash

# Author: Sosnin V.V. 

# Debug settings for the entire script
export PS4='+${BASH_SOURCE}:${LINENO}): ${FUNCNAME[0]:+${FUNCNAME[0]}(): }' # show the line number of executed commands
set -o nounset  # forbid using uninitialized variables
set -o errexit  # stop the scriot on any error
set -o pipefail # detect errors in pipelines
set -o xtrace   # print every executed command (this helps debugging and also allows to add all scripts variables to log file)
# set -o verbose  # show substitutions and expansions

# Log file for storing experiment configuration
ConfigFileName="logs/config-`date +%s`.txt"
LogFileName="logs/nresults-`date +%s`.txt"
printf "Experiment parameters \n\n" >> $ConfigFileName
echo "Setting up the experiment. See log file for details."

exec 3>&1 4>&2 >> $ConfigFileName 2>&1 # Redirect stdout/stderr to log file


# Test bed parameters
RoundTripTimeMs=10
FullBandwidthMbps=100 # This value must be close to channel bandwidth
AuxBandwidthMbps=1.5 # Reserved bandwidth for small auxiliary traffic that doesn't fall into filters (must be at least 100Kbps)

Qdisc=htb # options: htb, fifo, hfsc

TxSpeedIperfMbps=110
UpperLimitMbps=100 # Means no limit at all

# Calculated parameters in hclock2hfsc (ls)
LS1=64
LS2=36

UL1=80
UL2=40

ExperimentScale=1.0
ExperimentStepSeconds=$(bc -l <<< "1*$ExperimentScale")
ExperimentOffsetSeconds=$(bc -l <<< "20*$ExperimentScale")
ExperimentDurationSeconds=$(bc -l <<< "60*$ExperimentScale")

PeerInterface=enp0s8
LocalInterface=enp0s8

PeerAddress=192.168.57.2 # Server adress

# UDP client settings for traffic generator
PacketSizeBytes=1460
BufferSizeBytes=1000000 

UdpPortRangeMin=50000
UdpPortRangeMax=50001

# Authentication info to prevent interactive root password input
PeerUserName=vm1
LocalSudoPassword=1234
PeerSudoPassword=1234

echo $LocalSudoPassword | sudo -S true

# Set round-trip time for network
ssh $PeerUserName@$PeerAddress \
  "echo $PeerSudoPassword | sudo -S \
  tc qdisc replace dev $PeerInterface root netem delay ${RoundTripTimeMs}ms"

# Setup queuing discipline
sudo tc qdisc del dev $LocalInterface root 2>&- || true # cleanup

case "$Qdisc" in
hfsc)
    sudo tc qdisc  add dev $LocalInterface root handle 1: hfsc default 999
    # For all unexpected traffic
    sudo tc class  add dev $LocalInterface parent 1: classid 1:1 hfsc ls rate ${FullBandwidthMbps}mbit ul rate ${FullBandwidthMbps}mbit
    sudo tc class  add dev $LocalInterface parent 1:1 classid 1:999 hfsc ls rate ${AuxBandwidthMbps}mbit ul rate ${AuxBandwidthMbps}mbit


    sudo tc class  add dev $LocalInterface parent 1:1 classid 1:11 hfsc ls rate ${LS1}mbit ul rate ${UL1}mbit # v1
    sudo tc filter add dev $LocalInterface prio 1 u32 match ip dport 50000 0xffff flowid 1:11
    sudo tc class  add dev $LocalInterface parent 1:1 classid 1:12 hfsc ls rate ${LS2}mbit ul rate ${UL2}mbit # v2
    sudo tc filter add dev $LocalInterface prio 1 u32 match ip dport 50001 0xffff flowid 1:12
    ;;
htb)
    sudo tc qdisc  add dev $LocalInterface root handle 1: htb default 999
    # For all unexpected traffic
    sudo tc class  add dev $LocalInterface parent 1: classid 1:1 htb rate ${FullBandwidthMbps}mbit ceil ${FullBandwidthMbps}mbit
    sudo tc class  add dev $LocalInterface parent 1:1 classid 1:999 htb rate ${AuxBandwidthMbps}mbit ceil ${AuxBandwidthMbps}mbit

    sudo tc class  add dev $LocalInterface parent 1:1 classid 1:11 htb rate ${LS1}mbit ceil ${UL1}mbit # v1
    sudo tc filter add dev $LocalInterface prio 1 u32 match ip dport 50000 0xffff flowid 1:11
    sudo tc class  add dev $LocalInterface parent 1:1 classid 1:12 htb rate ${LS2}mbit ceil ${UL2}mbit # v2
    sudo tc filter add dev $LocalInterface prio 1 u32 match ip dport 50001 0xffff flowid 1:12

    ;;
fifo)
    echo "Using default qdisc for BLT: pfifo-fast"
    ;;
*)
    echo "Chosen Qdisc is not supported $Qdisc"
    exit
esac

# Run iperf3 servers
ssh $PeerUserName@$PeerAddress "killall iperf3 2>&- || true"
for ((port=$UdpPortRangeMin; port<=$UdpPortRangeMax; port++)); do
    ssh $PeerUserName@$PeerAddress "iperf3 -s -p$port -V -i${ExperimentStepSeconds} -1 --json" >> ${LogFileName}.$port &
done

# Restore stdout and stderr. This prevents writing iperf3 client output to
# Log file (because we only need server output in JSON format).
exec 1>&3 2>&4

echo "Waiting for iperf3 servers to start..."
for ((i=1; i>=0; i--)); do sleep 1; echo $i; done

iperf3 -c$PeerAddress -p50001 -t${ExperimentDurationSeconds} -i${ExperimentStepSeconds} -u \
    -w${BufferSizeBytes} -l${PacketSizeBytes} -b${TxSpeedIperfMbps}M &
iperf3 -c$PeerAddress -p50000 -t30 -i${ExperimentStepSeconds} -u \
-w${BufferSizeBytes} -l${PacketSizeBytes} -b${TxSpeedIperfMbps}M &


sleep $ExperimentDurationSeconds 20

echo "Experiment ended."