package com.hitzri.pipelinerobot.service;

import com.hitzri.pipelinerobot.config.StorageProperties;
import com.hitzri.pipelinerobot.vo.OtaUploadResponse;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.HexFormat;
import java.util.Locale;
import java.util.Set;
import java.util.UUID;
import org.springframework.core.io.FileSystemResource;
import org.springframework.core.io.Resource;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.stereotype.Service;
import org.springframework.util.StringUtils;
import org.springframework.web.multipart.MultipartFile;

@Service
public class OtaService {

    private static final Set<String> ALLOWED_TARGETS = Set.of("mobile_part", "fixed_part");
    private static final DateTimeFormatter FILE_TIMESTAMP =
        DateTimeFormatter.ofPattern("yyyyMMddHHmmss", Locale.ROOT);

    private final StorageProperties storageProperties;

    public OtaService(StorageProperties storageProperties) {
        this.storageProperties = storageProperties;
    }

    public OtaUploadResponse storeFirmware(String target, MultipartFile file) throws IOException {
        String normalizedTarget = normalizeTarget(target);
        if (file == null || file.isEmpty()) {
            throw new IllegalArgumentException("请上传 .bin 固件文件");
        }

        String originalFilename = StringUtils.cleanPath(String.valueOf(file.getOriginalFilename()));
        if (!StringUtils.hasText(originalFilename) || !originalFilename.toLowerCase(Locale.ROOT).endsWith(".bin")) {
            throw new IllegalArgumentException("仅支持上传 .bin 固件文件");
        }

        Path otaRoot = resolveOtaRootPath();
        Files.createDirectories(otaRoot);

        String storedName = buildStoredName(normalizedTarget, originalFilename);
        Path storedPath = otaRoot.resolve(storedName).normalize();
        if (!storedPath.startsWith(otaRoot)) {
            throw new IllegalArgumentException("无效的 OTA 存储路径");
        }

        Files.copy(file.getInputStream(), storedPath, StandardCopyOption.REPLACE_EXISTING);

        OtaUploadResponse response = new OtaUploadResponse();
        response.setJobId(UUID.randomUUID().toString());
        response.setTarget(normalizedTarget);
        response.setFilename(originalFilename);
        response.setStoredName(storedName);
        response.setSize(Files.size(storedPath));
        response.setSha256(calculateSha256(storedPath));
        response.setUrl(buildDownloadUrl(storedName));
        return response;
    }

    public ResponseEntity<Resource> getFirmwareFile(String storedName) throws IOException {
        Path filePath = resolveFirmwarePath(storedName);
        if (!Files.exists(filePath) || !Files.isRegularFile(filePath)) {
            throw new java.nio.file.NoSuchFileException("OTA 固件不存在");
        }

        Resource resource = new FileSystemResource(filePath);
        return ResponseEntity.ok()
            .contentType(MediaType.APPLICATION_OCTET_STREAM)
            .contentLength(Files.size(filePath))
            .body(resource);
    }

    public Path resolveOtaRootPath() {
        return Path.of(storageProperties.getRootDir(), storageProperties.getOtaDir()).toAbsolutePath().normalize();
    }

    public Path resolveFirmwarePath(String storedName) {
        String normalizedName = StringUtils.cleanPath(String.valueOf(storedName));
        if (!StringUtils.hasText(normalizedName) || normalizedName.contains("..") || normalizedName.contains("/")) {
            throw new IllegalArgumentException("无效的 OTA 文件名");
        }
        Path otaRoot = resolveOtaRootPath();
        Path filePath = otaRoot.resolve(normalizedName).normalize();
        if (!filePath.startsWith(otaRoot)) {
            throw new IllegalArgumentException("无效的 OTA 文件路径");
        }
        return filePath;
    }

    private String normalizeTarget(String target) {
        String normalizedTarget = String.valueOf(target).trim();
        if (!ALLOWED_TARGETS.contains(normalizedTarget)) {
            throw new IllegalArgumentException("无效的 OTA 目标设备");
        }
        return normalizedTarget;
    }

    private String buildStoredName(String target, String originalFilename) {
        String baseName = originalFilename.replaceAll("[^A-Za-z0-9._-]", "_");
        return target + "-" + FILE_TIMESTAMP.format(LocalDateTime.now()) + "-" + baseName;
    }

    private String buildDownloadUrl(String storedName) {
        String baseUrl = String.valueOf(storageProperties.getOtaPublicBaseUrl()).replaceAll("/+$", "");
        return baseUrl + "/ota/files/" + storedName;
    }

    private String calculateSha256(Path filePath) throws IOException {
        MessageDigest digest;
        try {
            digest = MessageDigest.getInstance("SHA-256");
        } catch (NoSuchAlgorithmException error) {
            throw new IllegalStateException("SHA-256 不可用", error);
        }

        try (InputStream inputStream = Files.newInputStream(filePath)) {
            byte[] buffer = new byte[8192];
            int read;
            while ((read = inputStream.read(buffer)) != -1) {
                digest.update(buffer, 0, read);
            }
        }

        return HexFormat.of().formatHex(digest.digest());
    }
}
