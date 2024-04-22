rm -rf dist
mkdir -p dist
docker build --platform=linux/amd64 -f dockerfile -o type=tar,dest=dist/linux-x86-64.tar .
docker build --platform=linux/arm64 -f dockerfile -o type=tar,dest=dist/linux-arm64.tar .