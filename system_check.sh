#!/bin/bash

function check_vtx_enabled() {
    VTX_OK="3,5,7"
    VTX=$(sudo modprobe msr | sudo rdmsr 0x3A)
    
    if [[ $VTX_OK == *$VTX* ]]; then
        echo "VT-x enabled - OK"
    else
        echo "VT-x not enabled - ERROR"
    fi
}

function check_x2apic() {
    X2APIC=$(lscpu | grep x2apic)
    
    if [ "$X2APIC" == "" ];then
        echo "No x2apic flag - OK"
    else
        echo "x2apic needs to disabled from grub menu (noxapic). -ERROR"
    fi
}

check_vtx_enabled
check_x2apic