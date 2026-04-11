package com.hitzri.pipelinerobot.controller;

import com.hitzri.pipelinerobot.dto.FinishInspectionSessionRequest;
import com.hitzri.pipelinerobot.dto.StartInspectionSessionRequest;
import com.hitzri.pipelinerobot.service.InspectionService;
import com.hitzri.pipelinerobot.service.InspectionSessionService;
import com.hitzri.pipelinerobot.vo.InspectionDetailVO;
import com.hitzri.pipelinerobot.vo.InspectionHistoryItemVO;
import com.hitzri.pipelinerobot.vo.InspectionMediaFileVO;
import com.hitzri.pipelinerobot.vo.InspectionSessionFinishVO;
import com.hitzri.pipelinerobot.vo.InspectionSessionStartVO;
import com.hitzri.pipelinerobot.vo.PageResponse;
import java.io.IOException;
import org.springframework.core.io.ByteArrayResource;
import org.springframework.http.ResponseEntity;
import org.springframework.security.core.Authentication;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/inspection")
public class InspectionController {

    private final InspectionService inspectionService;
    private final InspectionSessionService inspectionSessionService;

    public InspectionController(
        InspectionService inspectionService,
        InspectionSessionService inspectionSessionService
    ) {
        this.inspectionService = inspectionService;
        this.inspectionSessionService = inspectionSessionService;
    }

    @GetMapping("/history")
    public PageResponse<InspectionHistoryItemVO> history(
        @RequestParam(required = false) String mode,
        @RequestParam(required = false) String environment,
        @RequestParam(required = false) String startDate,
        @RequestParam(required = false) String endDate,
        @RequestParam(defaultValue = "1") long page,
        @RequestParam(defaultValue = "10") long pageSize
    ) {
        return inspectionService.getHistory(mode, environment, startDate, endDate, page, pageSize);
    }

    @GetMapping("/{id}")
    public InspectionDetailVO detail(@PathVariable Long id) {
        return inspectionService.getDetail(id);
    }

    @GetMapping("/{id}/export")
    public ResponseEntity<ByteArrayResource> export(@PathVariable Long id) throws IOException {
        return inspectionService.exportRecord(id);
    }

    @PostMapping("/session/start")
    public InspectionSessionStartVO startSession(
        @RequestBody StartInspectionSessionRequest request,
        Authentication authentication
    ) throws IOException {
        String operator = authentication != null ? authentication.getName() : "system";
        String sessionId = inspectionSessionService.startSession(request.getMode(), request.getEnvironment(), operator);
        return new InspectionSessionStartVO(sessionId);
    }

    @PostMapping("/session/{sessionId}/capture")
    public InspectionMediaFileVO capture(@PathVariable String sessionId) throws IOException, InterruptedException {
        return inspectionSessionService.captureFrame(sessionId);
    }

    @PostMapping("/session/{sessionId}/finish")
    public InspectionSessionFinishVO finish(
        @PathVariable String sessionId,
        @RequestBody(required = false) FinishInspectionSessionRequest request
    ) throws IOException, InterruptedException {
        boolean copyDefectImages = request != null && request.isCopyDefectImages();
        Long inspectionId = inspectionSessionService.finishSession(sessionId, copyDefectImages);
        return new InspectionSessionFinishVO(inspectionId);
    }

    @PostMapping("/session/{sessionId}/abort")
    public void abort(@PathVariable String sessionId) throws IOException {
        inspectionSessionService.abortSession(sessionId);
    }
}
