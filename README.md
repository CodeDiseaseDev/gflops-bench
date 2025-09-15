# gflops-bench

### Usage
```
Command line parameters:
	-t | --threads <number of threads>
	-d | --duration <number of seconds>
	-b | --brutal  (Hits the CPU harder; requires root on Linux)
	-v | --vector  (Does vector calculations instead of addition)
	-h | --help
```
### Recommended Usage
`sudo gflops-bench -d 15 -b -v`

Runs at full tilt with brutal mode, and vector mode for **15** seconds. (sudo required on Linux)

# How to build
```bash
git clone https://github.com/CodeDiseaseDev/gflops-bench
cd gflops-bench
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
