package com.hitzri.pipelinerobot.controller;

import com.fasterxml.jackson.databind.JsonNode;
import com.hitzri.pipelinerobot.service.InspectionService;
import java.nio.file.NoSuchFileException;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
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

    private static final Logger log = LoggerFactory.getLogger(FileController.class);

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

    @GetMapping("/pipe-dataset/{stopId}/cloud_accum.pcd")
    public ResponseEntity<Resource> getPipeDatasetPointCloud(@PathVariable String stopId) throws IOException {
        Path path = inspectionService.resolvePipeDatasetPointCloudPath(stopId);
        log.info("Serving test capture point cloud: stopId={}, path={}", stopId, path);
        if (!Files.exists(path)) {
            log.warn("Point cloud file missing: stopId={}, path={}", stopId, path);
            throw new NoSuchFileException("点云文件不存在: " + path);
        }

        return ResponseEntity.ok()
            .contentType(MediaType.APPLICATION_OCTET_STREAM)
            .body(new FileSystemResource(path));
    }

    @GetMapping("/pipe-dataset/defects_global.json")
    public ResponseEntity<JsonNode> getPipeDatasetFittedResult() throws IOException {
        Path path = inspectionService.resolvePipeDatasetFittedResultPath();
        log.info("Serving fitted pipe result: path={}", path);
        JsonNode result = inspectionService.readPipeDatasetFittedResult();
        return ResponseEntity.ok()
            .contentType(MediaType.APPLICATION_JSON)
            .body(result);
    }
}
