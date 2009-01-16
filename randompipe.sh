#!/bin/sh

case `uname -s` in
SunOS)
	(ps -Al &
	 ls -alR /proc &
	 w &
	 date &) 2>&1
	;;
AIX)
	(ps -Al &
	 ls -alR /tmp &
	 w &
	 date &) 2>&1;
	;;
Linux)
	(ls -alR /proc &
         w &
         ps -Al &
         date &) 2>&1
	;;
*)
	(ls -alR /tmp &
	 w &
	 date &) 2>&1
esac
