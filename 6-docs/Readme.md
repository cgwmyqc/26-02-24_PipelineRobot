# ROS2 测试命令

开始自动运行：

```
ros2 topic pub /fixed_controller/start_auto std_msgs/msg/Bool "{data: true}" --once
```

检测完成：

```
ros2 topic pub /fixed_controller/detect_done std_msgs/msg/Bool "{data: true}" --once
```

看编码器：

```
ros2 topic echo /fixed_controller/encoder_count
```

看位移：

```
ros2 topic echo /fixed_controller/travel_m
```

看到位事件：

```
ros2 topic echo /fixed_controller/motion_reached
```





# 声呐Demo

1、先用QTCreator生成基础基于CMake的项目。

2、用VSCode打开该项目。

3、为vscode指定qt编译路径。

在.vscode/settings.json中指定：

```
{
    "cmake.configureSettings": {
        "CMAKE_PREFIX_PATH": "E:/QT5/5.12.12/5.12.12/msvc2017_64"
    }    
}
```



4、Cmake插件下方指定编译模式为release，我使用的是MSVC2019编译的，编译Debug有bug

5、Cmake插件下方选色工具包：指定MSVC2019 X86_64编译套件

6、点击生成。

7、使用QT提供的windeployqt工具拷贝运行时需要的库。(上面第4条的bug就是出在此出，使用qt提供的工具在拷贝debug版的dll时，platforms中qwindows.dll会拷贝成release版本的，需要手动拷贝debug版本的，解决方案见注意)

```shell
# 进入编译出的release目录
cd ./build/Realse

# 使用工具自动拷贝
E:\QT5\5.12.12\5.12.12\msvc2017_64\bin\windeployqt.exe 7-sonarDemo.exe
E:\QT5\5.12.12\5.12.12\msvc2017_64\bin\windeployqt.exe 7-sonarDemo.exe
```

注意：若使用debug版本，windeployqt工具向platforms中拷贝的是release版本的qwindows.dll，应该拷贝的是`E:\QT5\5.12.12\5.12.12\msvc2017_64\plugins\platforms`中的qwindowsd.dll，手动拷贝进去，就行了。

![](.\Asset\1.png)

8、拷贝完成后就可以点击运行按钮运行工程了



# 数据库
账户：root
密码：空

账户：dev
密码：123456

创建dev账户：
```docker exec -it pipeline_robot_mysql mysql -uroot -p```

查看用户权限：

```SELECT user, host FROM mysql.user;```

```

mysql> SELECT user, host FROM mysql.user;
+------------------+-----------+
| user             | host      |
+------------------+-----------+
| root             | %         |
| mysql.infoschema | localhost |
| mysql.session    | localhost |
| mysql.sys        | localhost |
| root             | localhost |
+------------------+-----------+

```

会发现root账户既有localhost访问权限又有远程访问权限%就可以，若root没有远程访问权限，需要创建一个dev账户添加远程访问权限。





注意：使用root账户默认时不允许远程访问的，所以要使用命令行先进入docker内的Mysql，然后创建一个dev账户。由于windows已经安装有mysql 3306端口占用了，所以用3307端口转发到docker的3306端口。









# Docker 通信关键点

ros1_bridge容器：

需要创建一个ros1_bridge容器用来转发ros1和ros2的话题，这个容器一般是以ubuntu20.04 ros1 noetic为主镜像，在其上面再安装humble以及ros1_birdge库，制作而成。其中关键点有几个：

1、   FASTDDS_BUILTIN_TRANSPORTS: UDPv4  ROS2的DDS中间件要使用UDPv4，使用默认原生的内存共享形式会造成ros2能看到ros1话题，但是echo没有数据.

2、ros2 run ros1_bridge dynamic_bridge 可以带上--bridge-all-topics也可以不带，带上表示对ros1和ros2话题进行全量转发，不带表示需要订阅后才会转发。

3、要先source ros1 ，再source ros2，再source ros1_bridge，否则会报错。

4、ros1 是有中心化通信，docker容器通信双方要指定ROS_MASTER_URI和ROS_IP，这样才能通信上。

**ros1_bridge容器的dockerfile:**

```
FROM ros:noetic-ros-base-focal

ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]

RUN apt-get update && apt-get install -y \
    locales tzdata curl gnupg2 lsb-release ca-certificates \
    build-essential cmake git wget vim \
    python3-pip python3-rosdep python3-argcomplete \
    python3-yaml python3-empy python3-nose python3-setuptools \
    libasio-dev libtinyxml2-dev libtinyxml-dev \
    && rm -rf /var/lib/apt/lists/*

RUN locale-gen en_US en_US.UTF-8
ENV LANG=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8

RUN rosdep init || true
RUN rosdep update

RUN python3 -m pip install --no-cache-dir -U \
    vcstool \
    colcon-common-extensions

WORKDIR /opt
RUN mkdir -p /opt/ros2_humble_ws/src
WORKDIR /opt/ros2_humble_ws

RUN wget https://raw.githubusercontent.com/ros2/ros2/humble/ros2.repos
RUN vcs import src < ros2.repos

RUN source /opt/ros/noetic/setup.bash && \
    apt-get update && \
    rosdep install --from-paths src --ignore-src -r -y \
      --skip-keys "fastcdr rti-connext-dds-6.0.1 urdfdom_headers" && \
    rm -rf /var/lib/apt/lists/*

RUN source /opt/ros/noetic/setup.bash && \
    colcon build \
      --symlink-install \
      --cmake-args -DCMAKE_BUILD_TYPE=Release

RUN mkdir -p /opt/bridge_ws/src
WORKDIR /opt/bridge_ws/src
RUN git clone https://github.com/ros2/ros1_bridge.git

WORKDIR /opt/bridge_ws

RUN source /opt/ros/noetic/setup.bash && \
    source /opt/ros2_humble_ws/install/setup.bash && \
    rosdep install --from-paths src --ignore-src -r -y && \
    colcon build \
      --symlink-install \
      --packages-select ros1_bridge \
      --cmake-force-configure \
      --cmake-args -DCMAKE_BUILD_TYPE=Release

RUN echo "source /opt/ros/noetic/setup.bash" >> /root/.bashrc && \
    echo "source /opt/ros2_humble_ws/install/setup.bash" >> /root/.bashrc && \
    echo "if [ -f /opt/bridge_ws/install/setup.bash ]; then source /opt/bridge_ws/install/setup.bash; fi" >> /root/.bashrc

COPY scripts/start.sh /usr/local/bin/start-ros1-ros2-bridge.sh
RUN chmod +x /usr/local/bin/start-ros1-ros2-bridge.sh

ENV ROS_MASTER_URI=http://ros1_camera_algo:11311
ENV ROS_DOMAIN_ID=0
ENV RMW_IMPLEMENTATION=rmw_fastrtps_cpp
ENV ROS1_INSTALL_PATH=/opt/ros/noetic
ENV ROS2_INSTALL_PATH=/opt/ros2_humble_ws/install

ENTRYPOINT ["/usr/local/bin/start-ros1-ros2-bridge.sh"]

```

**docker compose:**

```
name: pipeline_robot

services:
  micro_ros_agent:
    build:
      context: ./micro-ros-agent
    container_name: micro_ros_agent
    restart: unless-stopped
    command: ["udp4", "--port", "8888", "-v6"]
    ports:
      - "${MICRO_ROS_AGENT_PORT:-8888}:8888/udp"
    networks:
      - pipeline_robot_net

  web_ros2_bridge:
    build:
      context: ./web-ros2-bridge
    container_name: web_ros2_bridge
    restart: unless-stopped
    environment:
      ROSBRIDGE_PORT: "${ROSBRIDGE_PORT:-9090}"
    ports:
      - "${ROSBRIDGE_PORT:-9090}:9090"
    networks:
      - pipeline_robot_net

  mysql:
    build:
      context: ./mysql
    container_name: pipeline_robot_mysql
    restart: unless-stopped
    command:
      - --default-authentication-plugin=mysql_native_password
      - --character-set-server=utf8mb4
      - --collation-server=utf8mb4_unicode_ci
    environment:
      MYSQL_ROOT_PASSWORD: "${MYSQL_ROOT_PASSWORD:-root}"
      MYSQL_DATABASE: "${MYSQL_DATABASE:-pipeline_robot}"
      MYSQL_USER: "${MYSQL_USER:-dev}"
      MYSQL_PASSWORD: "${MYSQL_PASSWORD:-123456}"
      TZ: "${TZ:-Asia/Shanghai}"
    ports:
      - "${MYSQL_HOST_PORT:-3307}:3306"
    volumes:
      - pipeline_robot_mysql_data:/var/lib/mysql
      - ./mysql/init:/docker-entrypoint-initdb.d:ro
    healthcheck:
      test:
        [
          "CMD",
          "mysqladmin",
          "ping",
          "-h",
          "127.0.0.1",
          "-uroot",
          "-p${MYSQL_ROOT_PASSWORD:-root}"
        ]
      interval: 10s
      timeout: 5s
      retries: 10
      start_period: 30s
    networks:
      - pipeline_robot_net

  ros1_camera_algo:
    build:
      context: ./ros1-camera-algo
    container_name: ros1_camera_algo
    restart: unless-stopped
    environment:
      ROS_MASTER_URI: "http://ros1_camera_algo:11311"
      ROS_HOSTNAME: ros1_camera_algo
      CAMERA_ALGO_START_COMMAND: "${CAMERA_ALGO_START_COMMAND:-}"
    volumes:
      - ./ros1-camera-algo/volumes/ros1_camera_ws:/opt/camera_ws
    networks:
      - pipeline_robot_net

  ros1_bridge:
    build:
      context: ./ros1-ros2-bridge
    container_name: ros1_bridge
    depends_on:
      - ros1_camera_algo
    environment:
      ROS_MASTER_URI: "http://ros1_camera_algo:11311"
      ROS1_INSTALL_PATH: /opt/ros/noetic
      ROS2_INSTALL_PATH: /opt/ros2_humble_ws/install
      ROS1_BRIDGE_COMMAND: "${ROS1_BRIDGE_COMMAND:-ros2 run ros1_bridge dynamic_bridge}"
      ROS_DOMAIN_ID: 0
      RMW_IMPLEMENTATION: rmw_fastrtps_cpp
      FASTDDS_BUILTIN_TRANSPORTS: UDPv4
      ROS_LOCALHOST_ONLY: 0
    command: ["ros2", "run", "ros1_bridge", "dynamic_bridge"]
    networks:
      - pipeline_robot_net
networks:
  pipeline_robot_net:
    driver: bridge

volumes:
  pipeline_robot_mysql_data:

```







# 海康鱼眼相机

iVMS登录用户名和密码

用户名：admin

密码：hitzri1013
