package com.hitzri.pipelinerobot.vo;

import java.time.LocalDateTime;

public class InspectionAnomalyImageVO {

    private Long id;
    private String anomalyType;
    private String remark;
    private String imageUrl;
    private LocalDateTime capturedAt;

    public Long getId() { return id; }
    public void setId(Long id) { this.id = id; }
    public String getAnomalyType() { return anomalyType; }
    public void setAnomalyType(String anomalyType) { this.anomalyType = anomalyType; }
    public String getRemark() { return remark; }
    public void setRemark(String remark) { this.remark = remark; }
    public String getImageUrl() { return imageUrl; }
    public void setImageUrl(String imageUrl) { this.imageUrl = imageUrl; }
    public LocalDateTime getCapturedAt() { return capturedAt; }
    public void setCapturedAt(LocalDateTime capturedAt) { this.capturedAt = capturedAt; }
}
