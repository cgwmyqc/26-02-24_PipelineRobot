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




鱼眼相机用户名：admin
鱼眼相机密码：hitzri1013

### IP分配
声呐：192.168.1.10

鱼眼相机：192.168.1.64

Lidar：192.168.1.20

上位机：192.168.1.132