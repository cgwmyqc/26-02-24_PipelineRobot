package com.hitzri.pipelinerobot.controller;

import com.hitzri.pipelinerobot.service.InspectionService;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import org.springframework.core.io.FileSystemResource;
import org.springframework.core.io.Resource;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/files")
public class FileController {

    private final InspectionService inspectionService;

    public FileController(InspectionService inspectionService) {
        this.inspectionService = inspectionService;
    }

    @GetMapping("/{category}/{filename:.+}")
    public ResponseEntity<Resource> getFile(@PathVariable String category, @PathVariable String filename) throws IOException {
        Path path = inspectionService.resolveStoragePath(category + "/" + filename);
        if (!Files.exists(path)) {
            throw new IllegalArgumentException("文件不存在");
        }
        MediaType mediaType = category.equals("videos") ? MediaType.APPLICATION_OCTET_STREAM : MediaType.IMAGE_JPEG;
        return ResponseEntity.ok().contentType(mediaType).body(new FileSystemResource(path));
    }
}
