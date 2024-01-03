TMS Remote API documentation for the latest version

### Contents

TMSRemote.proto &ndash; defines TMS Remote gRPC service

*.proto &ndash; other proto files used by TMSRemote.proto

java-sample/TMSClientApp.java &ndash; java sample

python-sample/sample.py &ndash; python sample

cs-sample/TMSClientApp.cs &ndash; C# sample

cpp-sample/ClientAppGrpc.cpp &ndash; C++ sample

### Instructions how to run Java sample with Java&Maven

1. Change current folder to _java-sample_  
`cd java-sample/`

2. Get cert.pem SSL certificate file from InfoReach and put it to the current folder

3. Run the following command to install dependencies and build a sample application
`mvn compile`

4. Run the following command to install dependencies, build **and run** a sample application
`mvn install`


### Instructions how to run Python sample

1. Change current folder to _python-sample_  
`cd python-sample/`

2. All dependencies from requirements.txt should be pre-installed  
`pip install -r requirements.txt`  
or  
`python -m pip install -r requirements.txt`

3. Need to generate gRPC code using command below  
`python -m grpc_tools.protoc --python_out=remote --grpc_python_out=remote --proto_path=.. TMSRemote.proto TMSRemoteCommon.proto TMSRemoteEvents.proto TMSRemoteRequests.proto TMSTradingRequests.proto`

4. (Python3 only) Convert gRPC files to Python3 compatible format using 2to3 script  
`2to3 remote/ -w -n`  
or  
`python -m lib2to3 remote/ -w -n`

5. Get cert.pem SSL certificate file from InfoReach and put it to the current folder

6. Run sample.py  
`python sample.py`

### Instructions how to run C# sample with .NET Core 2.1

1. Change current folder to _cs-sample_  
`cd cs-sample/`

2. Get cert.pem SSL certificate file from InfoReach and put it to the current folder

3. Run the following command to install dependencies and build a sample application
`dotnet build`

4. Run the sample application
`dotnet run`

### Instructions how to run C++ sample

#### Windows

0. Prerequisites:
   - Git
   - CMake 3.13 or greater
   - Visual Studio 2015 Update 3 or greater

1. Install _vcpkg_ package manager to any convenient location (e.g. _%TOOLS_DIR%_)
```
cd %TOOLS_DIR%
git clone https://github.com/microsoft/vcpkg
.\vcpkg\bootstrap-vcpkg.bat
```

2. Install gRPC libraries
```
.\vcpkg\vcpkg.exe install grpc
```

3. Build a sample application (here _%SAMPLES_HOME%_ is a directory with this readme file)
```
cd %SAMPLES_HOME%\cpp-sample
mkdir build
cmake -B build -S . "-DCMAKE_TOOLCHAIN_FILE=%TOOLS_DIR%/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

(Build config _Release_ can be replaced with _Debug_)

4. Get cert.pem SSL certificate file from InfoReach and put it to the current folder

5. Run the application
```
.\build\Release\tms_client_app.exe
```

#### Linux

0. Prerequisites:
   - Git
   - CMake 3.13 or greater

1. Install tools required to build gRPC
```
sudo apt install -y build-essential autoconf libtool pkg-config
```

2. Download and install gRPC libraries to any convenient directory (e.g. _$GRPC_HOME_)
```
cd $GRPC_HOME
git clone --recurse-submodules -b v1.50.1 --depth 1 --shallow-submodules https://github.com/grpc/grpc
mkdir grpc/build
cd grpc/build
cmake .. -DCMAKE_BUILD_TYPE=Release -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=$GRPC_HOME
make -j 4
make install
export PATH="$GRPC_HOME/bin:$PATH"
```

3. Build a sample application (here _$SAMPLES_HOME_ is a directory with this readme file)
```
cd $SAMPLES_HOME/cpp-sample
mkdir build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

4. Get cert.pem SSL certificate file from InfoReach and put it to the current folder

5. Run the application
```
./build/tms_client_app
```
