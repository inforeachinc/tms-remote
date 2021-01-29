TMS Remote API documentation for version 9.0

### Contents
TMSRemote.proto - defines TMS Remote gRPC service

*.proto - other proto files used by TMSRemote.proto

java-sample/TMSClientApp.java - java sample

python-sample/sample.py - python sample

cs-sample/TMSClientApp.cs - C# sample

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