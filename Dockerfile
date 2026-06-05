FROM alpine:latest AS builder

RUN apk add --no-cache build-base linux-headers

WORKDIR /build

COPY *.c .
COPY *.h .
COPY *.o .
COPY Makefile .

RUN make build

FROM alpine:latest

RUN apk add --no-cache gcompat

WORKDIR /app

COPY --from=builder /build/server .
COPY ivf.bin .

ENTRYPOINT ["./server"]
