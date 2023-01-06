#!/usr/bin/sh

gcc pacc.c pacc_const.c test.c -Iinclude -Ofast -s -o test
