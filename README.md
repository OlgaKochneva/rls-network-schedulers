# rls-network-schedulers

### General

The presented schedulers map the parameters in hClock's (R-reservation, L-limit, S-shares) [1] style, to the parameters used in HFSC [3] and HTB [4]. The bandwidth distribution implemented according to [SUM(R, S)](#sumrs-semantics) or [ MAX(R, S)](#maxrs-semantics) semantics.

**Properties:**

* Availability
* Easy integration
* Bandwidth reservation
* Weighted distribution according to shares
* Bandwidth limiting
* Admission control
* Class hierarchy support (in development)

### SUM(R,S) semantics

TBD

### MAX(R,S) semantics

TBD

### Sources

---

1. Billaud J. P., Gulati A. hClock: Hierarchical QoS for packet schedul-
   ing in a hypervisor //Proceedings of the 8th ACM European Conference on Com-
   puter Systems. – 2013. – С. 309-322.

2. tc man page - https://man7.org/linux/man-pages/man8/tc.8.html

3. tc-hfsc man page - https://man7.org/linux/man-pages/man8/tc-htb.8.html

4. tc-htb man page - https://man7.org/linux/man-pages/man8/tc-hfsc.8.html

   
