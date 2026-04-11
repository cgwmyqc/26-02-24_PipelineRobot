package com.hitzri.pipelinerobot.dto;

public class StartInspectionSessionRequest {

    private String mode;
    private String environment;

    public String getMode() {
        return mode;
    }

    public void setMode(String mode) {
        this.mode = mode;
    }

    public String getEnvironment() {
        return environment;
    }

    public void setEnvironment(String environment) {
        this.environment = environment;
    }
}
