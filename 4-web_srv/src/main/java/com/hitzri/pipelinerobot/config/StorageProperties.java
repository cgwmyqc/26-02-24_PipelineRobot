package com.hitzri.pipelinerobot.config;

import org.springframework.boot.context.properties.ConfigurationProperties;

@ConfigurationProperties(prefix = "app.storage")
public class StorageProperties {

    private String rootDir;
    private String videoDir;
    private String imageDir;
    private String pointsDir;
    private String resultsDir;
    private String sessionsDir;
    private String otaDir;
    private String pipeDatasetRoot;
    private String publicUrlPrefix;
    private String otaPublicBaseUrl;
    private String ffmpegBin;
    private String cameraRtspUrl;

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

    public String getPointsDir() {
        return pointsDir;
    }

    public void setPointsDir(String pointsDir) {
        this.pointsDir = pointsDir;
    }

    public String getResultsDir() {
        return resultsDir;
    }

    public void setResultsDir(String resultsDir) {
        this.resultsDir = resultsDir;
    }

    public String getSessionsDir() {
        return sessionsDir;
    }

    public void setSessionsDir(String sessionsDir) {
        this.sessionsDir = sessionsDir;
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

    public String getFfmpegBin() {
        return ffmpegBin;
    }

    public void setFfmpegBin(String ffmpegBin) {
        this.ffmpegBin = ffmpegBin;
    }

    public String getCameraRtspUrl() {
        return cameraRtspUrl;
    }

    public void setCameraRtspUrl(String cameraRtspUrl) {
        this.cameraRtspUrl = cameraRtspUrl;
    }
}
