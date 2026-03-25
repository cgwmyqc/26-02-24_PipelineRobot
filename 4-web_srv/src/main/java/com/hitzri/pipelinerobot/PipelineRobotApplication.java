package com.hitzri.pipelinerobot;

import org.mybatis.spring.annotation.MapperScan;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.boot.context.properties.ConfigurationPropertiesScan;

@SpringBootApplication
@MapperScan("com.hitzri.pipelinerobot.mapper")
@ConfigurationPropertiesScan
public class PipelineRobotApplication {

    public static void main(String[] args) {
        SpringApplication.run(PipelineRobotApplication.class, args);
    }
}
