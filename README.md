# Silicom Bypass Control Module & Utilities

Silicom Bypass Control Module & Utilities mirror with redBorder & PF_RING
patches:

* Automatic bypass activation with PF_RING link change detection
* No module unloading when no bypass devices found (useful for virtual
machines)

## Build RPM
We use mock to build src and binary execute:
```
make rpm
```

The result rpms will be placed in ./packaging/rpm/pkgs/

## Build DKMS RPM
We use rpmbuild to build kernel module dkms src and binary rpm execute:

```
cd kernel
make rpm
```

In this case the rpms will be place in /var/lib/dkms/bpctl/<version>/rpm/

Note: Originaly the dkms rpm was built using `dmks mkrpm`, since this is gone in new versions we are building with rpmbuild. TODO: Will be cool to do it with mock too

