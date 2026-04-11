package com.hitzri.pipelinerobot.config;

import org.springframework.boot.context.properties.ConfigurationProperties;

@ConfigurationProperties(prefix = "app.storage")
public class StorageProperties {

    private String rootDir;
    private String videoDir;
    private String imageDir;
    private String otaDir;
    private String pipeDatasetRoot;
    private String publicUrlPrefix;
    private String otaPublicBaseUrl;

    public String getRootDir() {
        return rootDir;
    }

    public void setRootDir(String rootDir) {
        this.rootDir = rootDir;
    }

    public String getVideoDir() {
        return videoDir;
    }

    public void setVideoDir(String videoDir) {
        this.videoDir = videoDir;
    }

    public String getImageDir() {
        return imageDir;
    }

    public void setImageDir(String imageDir) {
        this.imageDir = imageDir;
    }

    public String getOtaDir() {
        return otaDir;
    }

    public void setOtaDir(String otaDir) {
        this.otaDir = otaDir;
    }

    public String getPipeDatasetRoot() {
        return pipeDatasetRoot;
    }

    public void setPipeDatasetRoot(String pipeDatasetRoot) {
        this.pipeDatasetRoot = pipeDatasetRoot;
    }

    public String getPublicUrlPrefix() {
        return publicUrlPrefix;
    }

    public void setPublicUrlPrefix(String publicUrlPrefix) {
        this.publicUrlPrefix = publicUrlPrefix;
    }

    public String getOtaPublicBaseUrl() {
        return otaPublicBaseUrl;
    }

    public void setOtaPublicBaseUrl(String otaPublicBaseUrl) {
        this.otaPublicBaseUrl = otaPublicBaseUrl;
    }
}
