FROM alpine:3.18

RUN apk add \
    build-base \
    expat \
    linux-headers \
    make \
    perl

ENV PLAN9=/usr/local/plan9

WORKDIR $PLAN9
COPY . .
RUN ./INSTALL

ENV PATH=$PATH:$PLAN9/bin
