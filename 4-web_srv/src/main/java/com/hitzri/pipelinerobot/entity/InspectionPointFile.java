package com.hitzri.pipelinerobot.entity;

import com.baomidou.mybatisplus.annotation.IdType;
import com.baomidou.mybatisplus.annotation.TableId;
import com.baomidou.mybatisplus.annotation.TableName;
import java.time.LocalDateTime;

@TableName("inspection_point_file")
public class InspectionPointFile {

    @TableId(type = IdType.AUTO)
    private Long id;
    private Long inspectionId;
    private String pointPath;
    private LocalDateTime capturedAt;

    public Long getId() { return id; }
    public void setId(Long id) { this.id = id; }
    public Long getInspectionId() { return inspectionId; }
    public void setInspectionId(Long inspectionId) { this.inspectionId = inspectionId; }
    public String getPointPath() { return pointPath; }
    public void setPointPath(String pointPath) { this.pointPath = pointPath; }
    public LocalDateTime getCapturedAt() { return capturedAt; }
    public void setCapturedAt(LocalDateTime capturedAt) { this.capturedAt = capturedAt; }
}
