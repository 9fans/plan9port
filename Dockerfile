FROM alpine:3.18

RUN apk update --no-cache

RUN apk add \
    gcc \
    libc-dev \
    linux-headers \
    perl

ENV PLAN9=/usr/local/plan9

WORKDIR $PLAN9
COPY . .
RUN ./INSTALL

ENV PATH=$PATH:$PLAN9/bin
