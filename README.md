# FLECS - The World of Automation
We make the world’s automation solutions available at one place, with one click!  

# How To Build
## Cloning the sources
If you are new to FLECS, make sure to clone recursively:
```
git clone --recurse-submodules https://github.com/flecs-technologies/flecs-public
```

If you have cloned the source code earlier, make sure to initialize submodules:
```
git submodule update --init
```

## Requirements
### The recommended way
It is recommended to use our official Docker image for building. To do so, run
```
docker run -it --rm --name flecs-build -v $(pwd):$(pwd) -w $(pwd) flecs/flecs-build:1.5.4-porpoise
```

from the the repository's root directory.

**Note:** It is recommended to use the Docker image tag that corresponds to the Git tag you are building.

### The other way
If you don't want to use our Docker image for whatever reason, please take a peek at https://github.com/FLECS-Technologies/flecs-build/tree/main/docker/flecs-build to find out which dependencies you might need for building. Mor detailed instructions will follow at a later stage...

## Building
1. Configuration:
```
cmake -G Ninja -B out/${ARCH} -DARCH=${ARCH}
```
`ARCH` has to be one of `(amd64|arm64|armhf)`

2a. Building the whole project
```
cmake --build out/${ARCH}
```

2b. Building single targets
```
cmake --build out/${ARCH} --target <target>
```

Relevant single `<target>`s might be `FLECS.flunder` or `FLECS.mqtt`.
