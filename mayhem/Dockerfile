FROM --platform=linux/amd64 ubuntu:20.04 as builder

RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y automake pkg-config libtool build-essential gettext libexpat1-dev autopoint clang

COPY . /repo
WORKDIR /repo
RUN ./autogen.sh
RUN ./configure --prefix=/install
RUN make -j8
RUN make install
RUN clang++ /repo/mayhem/fuzz.cpp -fsanitize=fuzzer,address -I /install/include/hangul-1.0/ -L /install/lib/ -l:libhangul.a -o /fuzz

FROM ubuntu:20.04 as package
RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y libexpat1-dev
COPY --from=builder /install /install
COPY --from=builder /fuzz /fuzz
