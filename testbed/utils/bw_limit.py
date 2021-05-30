import subprocess
import argparse

"""
Simple script to manage BW of virtual machines

VBoxManage bandwidthctl "VM name" add Limit --type network --limit 20m
VBoxManage modifyvm "VM name" --nicbandwidthgroup1 Limit
VBoxManage modifyvm "VM name" --nicbandwidthgroup2 Limit

VBoxManage bandwidthctl "VM name" set Limit --limit 100k


VBoxManage bandwidthctl "VM name" set Limit --limit 0


"""

def set_bw_limit_nic(vms, limit_mbps):
    for vm_name, nic in vms.items():
        output = subprocess.check_output(f'VBoxManage bandwidthctl "{vm_name}" add Limit --type network --limit {limit_mbps}m', shell=True)
        print(output)
        output = subprocess.check_output(f'VBoxManage modifyvm "{vm_name}" --nicbandwidthgroup{nic} Limit', shell=True)
        print(output)


def update_bw_limit(vms, limit_mbps):
    for vm_name in vms.keys():
        output = subprocess.check_output(f'VBoxManage bandwidthctl {vm_name} set Limit --limit {limit_mbps}m', shell=True)
        print(output)


def remove_bw_limit(vms):
    for vm_name in vms.keys():
        output = subprocess.check_output(f'VBoxManage bandwidthctl {vm_name} set Limit --limit 0', shell=True)
        print(output)


if __name__ == '__main__':
    vms = {"\"Ubuntu-18.04 Client\"": 2, "\"Ubuntu-18.04 Server\"": 2}
    limit_mbps = 100
    update_bw_limit(vms, limit_mbps)
