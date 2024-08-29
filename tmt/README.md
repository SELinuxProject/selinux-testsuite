# TMT test plans for selinux-testsuite

This directory contains basic "test plans" for running the selinux-testsuite via the [TMT tool](https://tmt.readthedocs.io/en/stable/). They are primarily intended for the GitHub-Actions-driven CI, but they can be also used directly through `tmt`:

```bash
tmt run [-e STS_ROOT_DOMAIN=...] [-e STS_KERNEL=...] \
    plans -f 'tag:-ci' --all provision -h ...
```

See `tmt run provision --help` for information about possible provisioning methods (most useful are `local`, `connect`, or `virtual.testcloud`).

Possible values for the `STS_ROOT_DOMAIN` env parameter are:
* `unconfined_t` - run the testsuite as an unconfined root.
* `sysadm_t` - run the testsuite as a `sysadm_u:sysadm_r:sysadm_t:...` root.

Possible values for the `STS_KERNEL` env parameter are:
* `default` - try to use the kernel currently booted on the test machine.
* `latest` - update to the latest kernel available in the repos and boot it.
* `secnext` - install the "secnext" kernel from https://repo.paul-moore.com/ and boot it.
