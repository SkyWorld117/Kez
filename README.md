# Fromager

## Structure of $FROMAGER_WORKDIR

```
$FROMAGER_WORKDIR
├── config.yaml
├── states.yaml
├── compilers.yaml
├── mpis.yaml
├── env
    ├── system
    ├── utilities
    ├── compilers
        ├── gcc-x.x.x
        ├── llvm-x.x.x
        ├── ...
    ├── mpis
        ├── openmpi-gcc-x.x.x-x.x.x
        ├── openmpi-llvm-x.x.x-x.x.x
        ├── ...
    ├── vendors
        ├── nvhpc-x.x
        ├── oneapi-x.x.x.x
        ├── ...
    ├── apps
        ├── ...
```

## Known Issues

- No actual AMD GPU support (ROCm etc.) as I do not have access to such hardware for testing. 
- Dependencies currently have no version constraints, but we may just leave it to the user to ensure compatibility. 
- No Python package support yet, but it is planned for the future.