package com.hitzri.pipelinerobot.vo;

public class InspectionSessionStartVO {

    private String sessionId;

    public InspectionSessionStartVO() {
    }

    public InspectionSessionStartVO(String sessionId) {
        this.sessionId = sessionId;
    }

    public String getSessionId() {
        return sessionId;
    }

    public void setSessionId(String sessionId) {
        this.sessionId = sessionId;
    }
}
