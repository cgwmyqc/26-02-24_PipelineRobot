package com.hitzri.pipelinerobot.vo;

import java.time.LocalDateTime;

public class InspectionMediaFileVO {

    private String fileName;
    private String filePath;
    private String fileUrl;
    private LocalDateTime capturedAt;

    public String getFileName() { return fileName; }
    public void setFileName(String fileName) { this.fileName = fileName; }
    public String getFilePath() { return filePath; }
    public void setFilePath(String filePath) { this.filePath = filePath; }
    public String getFileUrl() { return fileUrl; }
    public void setFileUrl(String fileUrl) { this.fileUrl = fileUrl; }
    public LocalDateTime getCapturedAt() { return capturedAt; }
    public void setCapturedAt(LocalDateTime capturedAt) { this.capturedAt = capturedAt; }
}
