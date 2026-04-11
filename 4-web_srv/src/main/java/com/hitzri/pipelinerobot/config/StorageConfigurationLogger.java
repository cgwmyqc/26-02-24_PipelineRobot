package com.hitzri.pipelinerobot.config;

import java.nio.file.Path;
import java.nio.file.Paths;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.CommandLineRunner;
import org.springframework.stereotype.Component;

@Component
public class StorageConfigurationLogger implements CommandLineRunner {

    private static final Logger log = LoggerFactory.getLogger(StorageConfigurationLogger.class);

    private final StorageProperties storageProperties;

    public StorageConfigurationLogger(StorageProperties storageProperties) {
        this.storageProperties = storageProperties;
    }

    @Override
    public void run(String... args) {
        log.info("Storage root directory: {}", resolvePath(storageProperties.getRootDir()));
        log.info("Storage results directory: {}", storageProperties.getResultsDir());
        log.info("Storage session directory: {}", storageProperties.getSessionsDir());
        log.info("Pipe dataset root directory: {}", resolvePath(storageProperties.getPipeDatasetRoot()));
        log.info("FFmpeg binary configured: {}", storageProperties.getFfmpegBin());
        log.info("Camera RTSP URL configured: {}", storageProperties.getCameraRtspUrl());
        log.info("PIPE_DATASET_ROOT environment variable: {}", System.getenv().getOrDefault("PIPE_DATASET_ROOT", "<not set>"));
    }

    private Path resolvePath(String rawPath) {
        return Paths.get(rawPath).toAbsolutePath().normalize();
    }
}
