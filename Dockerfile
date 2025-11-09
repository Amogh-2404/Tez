# Multi-stage Dockerfile for Tez Web Server
# Stage 1: Build
FROM alpine:latest AS builder

# Install build dependencies
RUN apk add --no-cache \
    g++ \
    cmake \
    make \
    boost-dev \
    nlohmann-json

# Set working directory
WORKDIR /app

# Copy source files
COPY CMakeLists.txt ./
COPY src/ ./src/
COPY include/ ./include/
COPY config.json ./
COPY static/ ./static/

# Build the project
RUN mkdir -p build && \
    cd build && \
    cmake .. && \
    make && \
    strip Tez

# Stage 2: Runtime
FROM alpine:latest

# Install runtime dependencies only
RUN apk add --no-cache \
    boost-system \
    boost-filesystem \
    libstdc++ && \
    adduser -D -s /bin/sh -h /app tez

# Set working directory
WORKDIR /app

# Copy binary and required files from builder
COPY --from=builder --chown=tez:tez /app/build/Tez ./
COPY --from=builder --chown=tez:tez /app/config.json ./
COPY --from=builder --chown=tez:tez /app/static ./static/

# Create log directory
RUN mkdir -p /app/logs && chown tez:tez /app/logs

# Switch to non-root user
USER tez

# Expose port
EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD wget --no-verbose --tries=1 --spider http://localhost:8080/health || exit 1

# Run the server
CMD ["./Tez"]
