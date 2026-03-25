package com.hitzri.pipelinerobot.vo;

import java.time.LocalDateTime;
import java.util.List;

public class InspectionDetailVO {

    private Long id;
    private String mode;
    private String environment;
    private String operator;
    private String result;
    private LocalDateTime inspectionTime;
    private LocalDateTime createdAt;
    private String videoUrl;
    private List<InspectionAnomalyImageVO> anomalies;

    public Long getId() { return id; }
    public void setId(Long id) { this.id = id; }
    public String getMode() { return mode; }
    public void setMode(String mode) { this.mode = mode; }
    public String getEnvironment() { return environment; }
    public void setEnvironment(String environment) { this.environment = environment; }
    public String getOperator() { return operator; }
    public void setOperator(String operator) { this.operator = operator; }
    public String getResult() { return result; }
    public void setResult(String result) { this.result = result; }
    public LocalDateTime getInspectionTime() { return inspectionTime; }
    public void setInspectionTime(LocalDateTime inspectionTime) { this.inspectionTime = inspectionTime; }
    public LocalDateTime getCreatedAt() { return createdAt; }
    public void setCreatedAt(LocalDateTime createdAt) { this.createdAt = createdAt; }
    public String getVideoUrl() { return videoUrl; }
    public void setVideoUrl(String videoUrl) { this.videoUrl = videoUrl; }
    public List<InspectionAnomalyImageVO> getAnomalies() { return anomalies; }
    public void setAnomalies(List<InspectionAnomalyImageVO> anomalies) { this.anomalies = anomalies; }
}
