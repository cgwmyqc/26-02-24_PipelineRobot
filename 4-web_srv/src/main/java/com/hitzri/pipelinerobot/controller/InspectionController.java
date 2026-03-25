package com.hitzri.pipelinerobot.controller;

import com.hitzri.pipelinerobot.service.InspectionService;
import com.hitzri.pipelinerobot.vo.InspectionDetailVO;
import com.hitzri.pipelinerobot.vo.InspectionHistoryItemVO;
import com.hitzri.pipelinerobot.vo.PageResponse;
import java.io.IOException;
import org.springframework.core.io.ByteArrayResource;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/inspection")
public class InspectionController {

    private final InspectionService inspectionService;

    public InspectionController(InspectionService inspectionService) {
        this.inspectionService = inspectionService;
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
}
