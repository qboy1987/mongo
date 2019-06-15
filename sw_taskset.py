# -*- coding: utf-8 -*-
import time
from subprocess import Popen, PIPE,check_output

process_num = [1,2,3,4,5,6,7,9,10,11,12,13,14,15]
using_process = []

def getprocess_num():
    print(using_process)
    if len(using_process) == len(process_num):
        return -1
    for n in process_num:
        if n not in using_process:
            return n
    return -1

def get_pid(name):
    return map(int,check_output(["pidof",name]).split())

def tastset_pid(pcore,pid):
    cmd='taskset -pc {} {}'.format(pcore,pid)
    print cmd
    p = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
    p.wait()
    print p.communicate()

old_pid_arr = {}
num_index = 0
while(True):
    map_list1 = []
    map_list2 = []
    try
        map_list1 = get_pid('cc1plus')
    except:
        print "not foud cc1plus"

    try:
        map_list2 = get_pid('cc1')
    except Exception as e:
        print "not foud cc1"
    map_list = map_list1 + map_list2
    if len(map_list):
        print 'not process to taskset'
    using_process = []
    cur_pid_arr = dict()
    for pid in map_list:
        if pid in old_pid_arr.keys():
           using_process.append(old_pid_arr[pid]['pn'])
           cur_pid_arr[pid]={'pn':old_pid_arr[pid]['pn']}
    for pid in map_list:
       if pid not in old_pid_arr.keys():
           pn = getprocess_num()
           if pn == -1:
               break
           tastset_pid(pn,pid)
           cur_pid_arr[pid]={'pn':pn}
           using_process.append(pn)
    old_pid_arr = cur_pid_arr
    time.sleep(5)