package com.hitzri.pipelinerobot.vo;

public class InspectionSessionFinishVO {

    private Long inspectionId;

    public InspectionSessionFinishVO() {
    }

    public InspectionSessionFinishVO(Long inspectionId) {
        this.inspectionId = inspectionId;
    }

    public Long getInspectionId() {
        return inspectionId;
    }

    public void setInspectionId(Long inspectionId) {
        this.inspectionId = inspectionId;
    }
}
