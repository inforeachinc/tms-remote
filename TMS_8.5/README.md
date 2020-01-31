TMS Remote API documentation for version 8.5

### Contents
TMSRemote.proto - defines TMS Remote gRPC service

*.proto - other proto files used by TMSRemote.proto

java-sample/TMSClientApp.java - java sample

python-sample/main.py - python sample

### Instructions how to run Python sample

1. All dependencies from requirements.txt should be pre-installed  
`pip install -r requirements.txt`  
or  
`python -m pip install -r requirements.txt`

2. Need to generate gRPC code using command below (from python-sample folder)  
`python -m grpc_tools.protoc --python_out=remote --grpc_python_out=remote --proto_path=.. TMSRemote.proto TMSRemoteCommon.proto TMSRemoteEvents.proto TMSRemoteRequests.proto TMSTradingRequests.proto`

3. (Python3 only) Convert gRPC files to Python3 compatible format using 2to3 script  
`2to3 remote/ -w -n`  
or  
`python -m lib2to3 remote/ -w -n`

4. Get cert.pem SSL certificate file from InfoReach and put it to the folder with sample

5. Run main.py  
`python main.py`
