# 确保这里用的是 1.24，与你 go.mod 一致
FROM golang:1.24 AS build

ENV GOPROXY=https://goproxy.cn,https://mirrors.aliyun.com/goproxy/,direct
ENV GOSUMDB=sum.golang.google.cn

WORKDIR /src

# 先拷贝 mod 文件，利用 Docker 缓存层
COPY go.mod go.sum ./
RUN go mod download

# 拷贝代码
COPY . .

# 编译生成静态二进制文件
RUN CGO_ENABLED=0 GOOS=linux go build -o /server ./cmd/server

# 运行阶段：使用最小化镜像
FROM gcr.io/distroless/static-debian11
COPY --from=build /server /server
EXPOSE 8080
ENTRYPOINT ["/server"]