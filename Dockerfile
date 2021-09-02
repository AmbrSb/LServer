FROM ubuntu:latest

ENV DEBIAN_FRONTEND noninteractive
RUN apt update && \
    apt -y install g++-10 cmake git libprotobuf-dev protobuf-compiler-grpc libgrpc++-dev libyaml-cpp-dev libgtest-dev libgmock-dev libasio-dev libtbb-dev
ADD . /lserver.git
RUN git clone https://github.com/AmbrSb/LServer.git /lserver && mkdir /lserver/build
WORKDIR /lserver/build
RUN cmake -H.. -B. -DCMAKE_CXX_COMPILER=`which g++-10` -DCMAKE_BUILD_TYPE=Release && make -j 12
ENTRYPOINT ./lserver ../config.yaml
EXPOSE 15001
EXPOSE 5050
