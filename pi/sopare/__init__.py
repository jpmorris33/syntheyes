#!/usr/bin/env python
# -*- coding: utf-8 -*-

##
##  This is a disgusting hack to make the SoPare voice recogniser
##  control the 'eyes' program via unix signals.  Since 'eyes' has
##  to be run as root for the SPI system to work, we have to run the
##  killall via sudo (!)
##
##  If this had been a successful experiment I'd have looked into running
##  eyes as a regular user and the sudo security hole would be unnecessary
##  That said, it would be running on a completely isolated embedded system
##  anyway so maybe it's not that awful.
##

import os

def run(readable_results, data, rawbuf):
    print readable_results
    if('sigh' in readable_results):
        os.system('sudo killall -SIGUSR2 eyes')
    if('dammit' in readable_results):
        os.system('sudo killall -SIGUSR1 eyes')
    if('angry' in readable_results):
        os.system('sudo killall -SIGUSR1 eyes')
    if('gasp' in readable_results):
        os.system('sudo killall -SIGPOLL eyes')

