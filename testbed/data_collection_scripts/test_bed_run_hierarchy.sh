#!/bin/bash

# Author: Sosnin V.V. & Kochneva O.R.

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
FullBandwidthKbps=94000 # This value must be close to channel bandwidth
AuxBandwidthKbps=1500 # Reserved bandwidth for small auxiliary traffic that doesn't fall into filters (must be at least 100Kbps)

Qdisc=hfsc # options: htb, fifo, hfsc

TxSpeedIperfKbps=100000
UpperLimitKbps=94000 # Means no limit at all

# Calculated parameters in hclock2hfsc (ls)
P1LowerLimitKbps=30000
V1LowerLimitKbps=20000
V2LowerLimitKbps=10000

P2LowerLimitKbps=29330
V3LowerLimitKbps=29330

P3LowerLimitKbps=20000
V4LowerLimitKbps=2000
V5LowerLimitKbps=18000

V6LowerLimitKbps=14670


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

UdpPortRangeMin=50001
UdpPortRangeMid=50004
UdpPortRangeMax=50006

# Authentication info to prevent interactive root password input
PeerUserName=vm1
LocalSudoPassword=1234
PeerSudoPassword=1234

echo $LocalSudoPassword | sudo -S true

# Setup queuing discipline
sudo tc qdisc del dev $LocalInterface root 2>&- || true # cleanup

# Set round-trip time for network
ssh $PeerUserName@$PeerAddress \
  "echo $PeerSudoPassword | sudo -S \
  tc qdisc replace dev $PeerInterface root netem delay ${RoundTripTimeMs}ms"

case "$Qdisc" in
hfsc)
    sudo tc qdisc  add dev $LocalInterface root handle 1: hfsc default 999
    # For all unexpected traffic
    sudo tc class  add dev $LocalInterface parent 1: classid 1:1 hfsc ls rate ${FullBandwidthKbps}kbit ul rate ${FullBandwidthKbps}kbit
    sudo tc class  add dev $LocalInterface parent 1:1 classid 1:999 hfsc ls rate ${AuxBandwidthKbps}kbit ul rate ${AuxBandwidthKbps}kbit

    sudo tc class  add dev $LocalInterface parent 1:1 classid 1:10 hfsc ls rate ${P1LowerLimitKbps}kbit ul rate ${FullBandwidthKbps}kbit # p1
    sudo tc class  add dev $LocalInterface parent 1:1 classid 1:20 hfsc ls rate ${P2LowerLimitKbps}kbit ul rate ${FullBandwidthKbps}kbit # p2
    sudo tc class  add dev $LocalInterface parent 1:1 classid 1:30 hfsc ls rate ${P3LowerLimitKbps}kbit ul rate ${FullBandwidthKbps}kbit # p3

    sudo tc class  add dev $LocalInterface parent 1:1 classid 1:16 hfsc ls rate ${V6LowerLimitKbps}kbit ul rate ${FullBandwidthKbps}kbit # v6
    sudo tc filter add dev $LocalInterface prio 1 u32 match ip dport 50006 0xffff flowid 1:16

    sudo tc class  add dev $LocalInterface parent 1:10 classid 1:11 hfsc ls rate ${V1LowerLimitKbps}kbit ul rate ${FullBandwidthKbps}kbit # v1
    sudo tc filter add dev $LocalInterface prio 1 u32 match ip dport 50001 0xffff flowid 1:11
    sudo tc class  add dev $LocalInterface parent 1:10 classid 1:12 hfsc ls rate ${V2LowerLimitKbps}kbit ul rate ${FullBandwidthKbps}kbit # v2
    sudo tc filter add dev $LocalInterface prio 1 u32 match ip dport 50002 0xffff flowid 1:12

    sudo tc class  add dev $LocalInterface parent 1:20 classid 1:13 hfsc ls rate ${V3LowerLimitKbps}kbit ul rate ${FullBandwidthKbps}kbit # v3
    sudo tc filter add dev $LocalInterface prio 1 u32 match ip dport 50003 0xffff flowid 1:13

    sudo tc class  add dev $LocalInterface parent 1:30 classid 1:14 hfsc ls rate ${V4LowerLimitKbps}kbit ul rate ${FullBandwidthKbps}kbit # v4
    sudo tc filter add dev $LocalInterface prio 1 u32 match ip dport 50004 0xffff flowid 1:14
    sudo tc class  add dev $LocalInterface parent 1:30 classid 1:15 hfsc ls rate ${V5LowerLimitKbps}kbit ul rate ${FullBandwidthKbps}kbit # v5
    sudo tc filter add dev $LocalInterface prio 1 u32 match ip dport 50005 0xffff flowid 1:15
    ;;
htb)
    sudo tc qdisc  add dev $LocalInterface root handle 1: htb default 999
    # For all unexpected traffic

    sudo tc class  add dev $LocalInterface parent 1: classid 1:1 htb rate ${FullBandwidthKbps}kbit ceil ${FullBandwidthKbps}kbit
    sudo tc class  add dev $LocalInterface parent 1:1 classid 1:999 htb rate ${AuxBandwidthKbps}kbit ceil ${AuxBandwidthKbps}kbit

    sudo tc class  add dev $LocalInterface parent 1:1 classid 1:10 htb rate ${P1LowerLimitKbps}kbit ceil ${FullBandwidthKbps}kbit # p1
    sudo tc class  add dev $LocalInterface parent 1:1 classid 1:20 htb rate ${P2LowerLimitKbps}kbit ceil ${FullBandwidthKbps}kbit # p2
    sudo tc class  add dev $LocalInterface parent 1:1 classid 1:30 htb rate ${P3LowerLimitKbps}kbit ceil ${FullBandwidthKbps}kbit # p3

    sudo tc class  add dev $LocalInterface parent 1:1 classid 1:16 htb rate ${V6LowerLimitKbps}kbit ceil ${FullBandwidthKbps}kbit # v6
    sudo tc filter add dev $LocalInterface prio 1 u32 match ip dport 50006 0xffff flowid 1:16

    sudo tc class  add dev $LocalInterface parent 1:10 classid 1:11 htb rate ${V1LowerLimitKbps}kbit ceil ${FullBandwidthKbps}kbit # v1
    sudo tc filter add dev $LocalInterface prio 1 u32 match ip dport 50001 0xffff flowid 1:11
    sudo tc class  add dev $LocalInterface parent 1:10 classid 1:12 htb rate ${V2LowerLimitKbps}kbit ceil ${FullBandwidthKbps}kbit # v2
    sudo tc filter add dev $LocalInterface prio 1 u32 match ip dport 50002 0xffff flowid 1:12

    sudo tc class  add dev $LocalInterface parent 1:20 0classid 1:13 htb rate ${V3LowerLimitKbps}kbit ceil ${FullBandwidthKbps}kbit # v3
    sudo tc filter add dev $LocalInterface prio 1 u32 match ip dport 50003 0xffff flowid 1:13

    sudo tc class  add dev $LocalInterface parent 1:30 classid 1:14 htb rate ${V4LowerLimitKbps}kbit ceil ${FullBandwidthKbps}kbit # v4
    sudo tc filter add dev $LocalInterface prio 1 u32 match ip dport 50004 0xffff flowid 1:14
    sudo tc class  add dev $LocalInterface parent 1:30 classid 1:15 htb rate ${V5LowerLimitKbps}kbit ceil ${FullBandwidthKbps}kbit # v5
    sudo tc filter add dev $LocalInterface prio 1 u32 match ip dport 50005 0xffff flowid 1:15
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
for ((i=10; i>=1; i--)); do sleep 1; echo $i; done

for ((port=$UdpPortRangeMin; port<=$UdpPortRangeMax; port++)); do
    iperf3 -c$PeerAddress -p$port -t60 -i${ExperimentStepSeconds} -u \
        -w${BufferSizeBytes} -l${PacketSizeBytes} -b${TxSpeedIperfKbps}K &
done

# sleep 30

# for ((port=$UdpPortRangeMid; port<=$UdpPortRangeMax; port++)); do
#     iperf3 -c$PeerAddress -p$port -t60 -i${ExperimentStepSeconds} -u \
#         -w${BufferSizeBytes} -l${PacketSizeBytes} -b${TxSpeedIperfKbps}M &
# done

sleep $ExperimentDurationSeconds 20

echo "Experiment ended."