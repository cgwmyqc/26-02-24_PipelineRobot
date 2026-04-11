package com.hitzri.pipelinerobot.controller;

import com.fasterxml.jackson.databind.JsonNode;
import com.hitzri.pipelinerobot.service.InspectionService;
import jakarta.servlet.http.HttpServletRequest;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.NoSuchFileException;
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

    @GetMapping("/media/**")
    public ResponseEntity<Resource> getMediaFile(HttpServletRequest request) throws IOException {
        String uri = request.getRequestURI();
        String marker = "/files/media/";
        int markerIndex = uri.indexOf(marker);
        if (markerIndex < 0) {
            throw new IllegalArgumentException("无效文件路径");
        }

        String relativePath = uri.substring(markerIndex + marker.length());
        Path path = inspectionService.resolveStoragePath(relativePath);
        if (!Files.exists(path) || !Files.isRegularFile(path)) {
            throw new NoSuchFileException("文件不存在: " + relativePath);
        }

        return ResponseEntity.ok()
            .contentType(resolveMediaType(path.getFileName().toString()))
            .body(new FileSystemResource(path));
    }

    @GetMapping("/{category}/{filename:.+}")
    public ResponseEntity<Resource> getLegacyFile(@PathVariable String category, @PathVariable String filename) throws IOException {
        String relativePath = category + "/" + filename;
        Path path = inspectionService.resolveStoragePath(relativePath);
        if (!Files.exists(path)) {
            throw new NoSuchFileException("文件不存在: " + relativePath);
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

    private MediaType resolveMediaType(String fileName) {
        String normalized = String.valueOf(fileName).toLowerCase();
        if (normalized.endsWith(".mp4")) {
            return MediaType.valueOf("video/mp4");
        }
        if (normalized.endsWith(".pcd")) {
            return MediaType.APPLICATION_OCTET_STREAM;
        }
        if (normalized.endsWith(".png")) {
            return MediaType.IMAGE_PNG;
        }
        return MediaType.IMAGE_JPEG;
    }
}
