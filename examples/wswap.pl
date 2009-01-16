#!/usr/bin/perl

my $wipe_cmd = "wipe -kfZ -q";
my $swapoff_cmd = "/sbin/swapoff";
my $swapon_cmd = "/sbin/swapon";
my $mkswap_cmd = "/sbin/mkswap";

if ($#ARGV != 0) {
  print STDERR "specify device special file to wipe\n";
  die;
}

my $to_wipe = $ARGV[0];

sub check_swap_in_proc_swap {
  open PROC, "</proc/swaps" or die "could not open /proc/swaps: $!";
  while (my $line = <PROC>) {
    if ($line =~ /^(\S+)\s+(\w+)\s+(\d+)\s/) {
      if ($1 == $to_wipe) {
	print "Device $to_wipe is listed as a swap device of type\n";
	print "$2 of size $3 in /proc/swaps.\n";
	close PROC;
	return 1;
      }
    }
  }
  print "Device $to_wipe is NOT listed in /proc/swaps\n";
  close PROC;
  return 0;
}

# check that $to_wipe is a swap partition listed in /etc/fstab...

sub check_swap_in_fstab {
  my $result = 0;

  open FSTAB, "</etc/fstab" or die "could not open /etc/fstab: $!";
  while (my $line = <FSTAB>) {
    next if (/^\#/ =~ $line); # ignore comments
    my ($name, $mountpoint, $type, $etc) =split (/\s+/, $line);
    if ($name eq $to_wipe) {
	if ($type eq "swap") {
	  print STDERR "Device $to_wipe is listed as a "
	    . "\"swap\" partition in /etc/fstab.\n";
	  $result = 1;
	  last;
	} else {
	  print STDERR "Device $to_wipe is listed as a \"$type\" partition "
	    . "in /etc/fstab,\nnot as a \"swap\" partition.\n";
	  $result = -1;
	  last;
	}
      }
  }
  
  close FSTAB;

  if (!$result) {
    print STDERR "Device $to_wipe is NOT listed in /etc/fstab.\n";
  }

  if ($result < 0) { $result = 0 };
  return $result;
}

if (&check_swap_in_fstab ()) {
  my $cookie1 = int (rand (10000));
  my $cookie2 = int (rand (100));
  my $cookie = $cookie1 + $cookie2;
  print "Read CAREFULLY. Do you want to WIPE (i.e. ERASE) the contents\n";
  print "of the special device:\n";
  print "\t\t$to_wipe ?\n";
  print "If yes, compute the sum $cookie1 + $cookie2 and type ";
  print "\"yes\" followed by\na space and the sum.\n\n";
  
  my $response = <STDIN>;
  my $on = 0;
  if ($response =~ /^yes $cookie$/) {
    print "\nAcknowledged, wiping $to_wipe.\n";
    if (&check_swap_in_proc_swap ()) {
      print "Turning off swap partition...\n";
      system ($swapoff_cmd . " " . $to_wipe) and
	die "Failed to turn off swap partition.\n";
      $on = 1;
    }
    system ($wipe_cmd . " " . $to_wipe) and die "Failed to wipe.\n";
    print "Recreating swap partition...\n";
    system ($mkswap_cmd . " " . $to_wipe) and
      die "Failed to recreate swap partition.\n";
    if ($on) {
      print "Turning on swap partition...\n";
      system ($swapon_cmd . " " . $to_wipe) and
	die "Failed to turn on swap partition.\n";
    }
    print "Success.\n";
  } else {
    print "\nNot acknowledged. Quitting.\n";
  }
}
