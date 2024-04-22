rm -rf dist
mkdir -p dist
docker build --platform=linux/amd64 -f dockerfile -o dist/linux-x86-64 .
docker build --platform=linux/arm64 -f dockerfile -o dist/linux-arm64 .