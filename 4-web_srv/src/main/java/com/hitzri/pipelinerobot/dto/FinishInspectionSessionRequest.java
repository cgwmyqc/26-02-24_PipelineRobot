package com.hitzri.pipelinerobot.dto;

public class FinishInspectionSessionRequest {

    private boolean copyDefectImages;
    private String pointCloudFileName;
    private String pointCloudPcdContent;

    public boolean isCopyDefectImages() {
        return copyDefectImages;
    }

    public void setCopyDefectImages(boolean copyDefectImages) {
        this.copyDefectImages = copyDefectImages;
    }

    public String getPointCloudFileName() {
        return pointCloudFileName;
    }

    public void setPointCloudFileName(String pointCloudFileName) {
        this.pointCloudFileName = pointCloudFileName;
    }

    public String getPointCloudPcdContent() {
        return pointCloudPcdContent;
    }

    public void setPointCloudPcdContent(String pointCloudPcdContent) {
        this.pointCloudPcdContent = pointCloudPcdContent;
    }
}
