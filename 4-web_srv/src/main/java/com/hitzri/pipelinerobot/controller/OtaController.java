package com.hitzri.pipelinerobot.controller;

import com.hitzri.pipelinerobot.service.OtaService;
import com.hitzri.pipelinerobot.vo.OtaUploadResponse;
import java.io.IOException;
import org.springframework.core.io.Resource;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.multipart.MultipartFile;

@RestController
@RequestMapping("/ota")
public class OtaController {

    private final OtaService otaService;

    public OtaController(OtaService otaService) {
        this.otaService = otaService;
    }

    @PostMapping("/firmware")
    public ResponseEntity<OtaUploadResponse> uploadFirmware(
        @RequestParam("target") String target,
        @RequestParam("file") MultipartFile file
    ) throws IOException {
        return ResponseEntity.ok(otaService.storeFirmware(target, file));
    }

    @GetMapping("/files/{storedName:.+}")
    public ResponseEntity<Resource> getFirmwareFile(@PathVariable String storedName) throws IOException {
        return otaService.getFirmwareFile(storedName);
    }
}
