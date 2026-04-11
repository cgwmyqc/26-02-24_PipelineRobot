package com.hitzri.pipelinerobot.dto;

public class FinishInspectionSessionRequest {

    private boolean copyDefectImages;

    public boolean isCopyDefectImages() {
        return copyDefectImages;
    }

    public void setCopyDefectImages(boolean copyDefectImages) {
        this.copyDefectImages = copyDefectImages;
    }
}
