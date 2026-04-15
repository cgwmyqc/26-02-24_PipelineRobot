package com.hitzri.pipelinerobot.dto;

public class FinishInspectionSessionRequest {

    private boolean copyDefectImages;
    private boolean copyFittedResult;
    private String fittedResultJsonContent;
    private String pointCloudFileName;
    private String pointCloudPcdContent;

    public boolean isCopyDefectImages() {
        return copyDefectImages;
    }

    public void setCopyDefectImages(boolean copyDefectImages) {
        this.copyDefectImages = copyDefectImages;
    }

    public boolean isCopyFittedResult() {
        return copyFittedResult;
    }

    public void setCopyFittedResult(boolean copyFittedResult) {
        this.copyFittedResult = copyFittedResult;
    }

    public String getFittedResultJsonContent() {
        return fittedResultJsonContent;
    }

    public void setFittedResultJsonContent(String fittedResultJsonContent) {
        this.fittedResultJsonContent = fittedResultJsonContent;
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
