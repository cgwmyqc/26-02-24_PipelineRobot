package com.hitzri.pipelinerobot.service;

import com.baomidou.mybatisplus.core.conditions.query.LambdaQueryWrapper;
import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.hitzri.pipelinerobot.config.StorageProperties;
import com.hitzri.pipelinerobot.entity.InspectionAnomalyImage;
import com.hitzri.pipelinerobot.entity.InspectionPointFile;
import com.hitzri.pipelinerobot.entity.InspectionRecord;
import com.hitzri.pipelinerobot.mapper.InspectionAnomalyImageMapper;
import com.hitzri.pipelinerobot.mapper.InspectionPointFileMapper;
import com.hitzri.pipelinerobot.mapper.InspectionRecordMapper;
import com.hitzri.pipelinerobot.vo.InspectionAnomalyImageVO;
import com.hitzri.pipelinerobot.vo.InspectionDetailVO;
import com.hitzri.pipelinerobot.vo.InspectionHistoryItemVO;
import com.hitzri.pipelinerobot.vo.InspectionMediaFileVO;
import com.hitzri.pipelinerobot.vo.PageResponse;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.time.LocalDate;
import java.time.LocalDateTime;
import java.time.LocalTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Pattern;
import java.util.stream.Stream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.core.io.ByteArrayResource;
import org.springframework.http.ContentDisposition;
import org.springframework.http.HttpHeaders;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.stereotype.Service;
import org.springframework.util.StringUtils;

@Service
public class InspectionService {
    private static final Pattern PIPE_STOP_ID_PATTERN = Pattern.compile("^stop_(000[1-9]|001[0-4])$");
    private static final Logger log = LoggerFactory.getLogger(InspectionService.class);

    private final InspectionRecordMapper inspectionRecordMapper;
    private final InspectionAnomalyImageMapper anomalyImageMapper;
    private final InspectionPointFileMapper pointFileMapper;
    private final StorageProperties storageProperties;
    private final ObjectMapper objectMapper;

    public InspectionService(
        InspectionRecordMapper inspectionRecordMapper,
        InspectionAnomalyImageMapper anomalyImageMapper,
        InspectionPointFileMapper pointFileMapper,
        StorageProperties storageProperties,
        ObjectMapper objectMapper
    ) {
        this.inspectionRecordMapper = inspectionRecordMapper;
        this.anomalyImageMapper = anomalyImageMapper;
        this.pointFileMapper = pointFileMapper;
        this.storageProperties = storageProperties;
        this.objectMapper = objectMapper;
    }

    public PageResponse<InspectionHistoryItemVO> getHistory(
        String mode,
        String environment,
        String startDate,
        String endDate,
        long page,
        long pageSize
    ) {
        LocalDateTime startTime = parseStartTime(startDate);
        LocalDateTime endTime = parseEndTime(endDate);
        LambdaQueryWrapper<InspectionRecord> wrapper = new LambdaQueryWrapper<>();
        wrapper.like(StringUtils.hasText(mode), InspectionRecord::getMode, mode)
            .like(StringUtils.hasText(environment), InspectionRecord::getEnvironment, environment)
            .ge(startTime != null, InspectionRecord::getInspectionTime, startTime)
            .le(endTime != null, InspectionRecord::getInspectionTime, endTime)
            .orderByDesc(InspectionRecord::getCreatedAt);

        Page<InspectionRecord> pageResult = inspectionRecordMapper.selectPage(new Page<>(page, pageSize), wrapper);
        List<InspectionHistoryItemVO> records = pageResult.getRecords().stream().map(this::toHistoryItem).toList();
        return new PageResponse<>(pageResult.getTotal(), records);
    }

    public InspectionDetailVO getDetail(Long id) {
        InspectionRecord record = inspectionRecordMapper.selectById(id);
        if (record == null) {
            throw new IllegalArgumentException("巡检记录不存在");
        }

        LambdaQueryWrapper<InspectionAnomalyImage> imageQuery = new LambdaQueryWrapper<>();
        imageQuery.eq(InspectionAnomalyImage::getInspectionId, id).orderByAsc(InspectionAnomalyImage::getCapturedAt);
        List<InspectionAnomalyImage> imageEntities = anomalyImageMapper.selectList(imageQuery);
        List<InspectionAnomalyImageVO> anomalies = imageEntities.stream().map(this::toAnomalyVO).toList();

        LambdaQueryWrapper<InspectionPointFile> pointQuery = new LambdaQueryWrapper<>();
        pointQuery.eq(InspectionPointFile::getInspectionId, id).orderByAsc(InspectionPointFile::getCapturedAt);
        List<InspectionPointFile> pointEntities = pointFileMapper.selectList(pointQuery);

        List<String> videoPaths = collectVideoPaths(record);
        List<String> imagePaths = collectImagePaths(record, imageEntities);
        List<String> pointPaths = collectPointPaths(record, pointEntities);

        Map<String, LocalDateTime> imageCapturedAt = new LinkedHashMap<>();
        for (InspectionAnomalyImage image : imageEntities) {
            if (StringUtils.hasText(image.getImagePath())) {
                imageCapturedAt.put(image.getImagePath(), image.getCapturedAt());
            }
        }

        Map<String, LocalDateTime> pointCapturedAt = new LinkedHashMap<>();
        for (InspectionPointFile pointFile : pointEntities) {
            if (StringUtils.hasText(pointFile.getPointPath())) {
                pointCapturedAt.put(pointFile.getPointPath(), pointFile.getCapturedAt());
            }
        }

        InspectionDetailVO vo = new InspectionDetailVO();
        vo.setId(record.getId());
        vo.setMode(record.getMode());
        vo.setEnvironment(record.getEnvironment());
        vo.setOperator(record.getOperatorName());
        vo.setResult(record.getResultSummary());
        vo.setInspectionTime(record.getInspectionTime());
        vo.setCreatedAt(record.getCreatedAt());
        vo.setVideoUrl(videoPaths.isEmpty() ? "" : buildPublicUrl(videoPaths.get(0)));
        vo.setAnomalies(anomalies);
        vo.setVideos(toMediaVOs(videoPaths, record.getInspectionTime()));
        vo.setImages(toMediaVOs(imagePaths, imageCapturedAt));
        vo.setPoints(toMediaVOs(pointPaths, pointCapturedAt));
        return vo;
    }

    public ResponseEntity<ByteArrayResource> exportRecord(Long id) throws IOException {
        InspectionRecord record = inspectionRecordMapper.selectById(id);
        if (record == null) {
            throw new IllegalArgumentException("巡检记录不存在");
        }

        LambdaQueryWrapper<InspectionAnomalyImage> imageQuery = new LambdaQueryWrapper<>();
        imageQuery.eq(InspectionAnomalyImage::getInspectionId, id).orderByAsc(InspectionAnomalyImage::getCapturedAt);
        List<InspectionAnomalyImage> imageEntities = anomalyImageMapper.selectList(imageQuery);

        LambdaQueryWrapper<InspectionPointFile> pointQuery = new LambdaQueryWrapper<>();
        pointQuery.eq(InspectionPointFile::getInspectionId, id).orderByAsc(InspectionPointFile::getCapturedAt);
        List<InspectionPointFile> pointEntities = pointFileMapper.selectList(pointQuery);

        List<String> videoPaths = collectVideoPaths(record);
        List<String> imagePaths = collectImagePaths(record, imageEntities);
        List<String> pointPaths = collectPointPaths(record, pointEntities);

        ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
        try (ZipOutputStream zipOutputStream = new ZipOutputStream(byteArrayOutputStream)) {
            for (String videoPath : videoPaths) {
                writeFileToZip(zipOutputStream, videoPath, "videos/");
            }
            for (String imagePath : imagePaths) {
                writeFileToZip(zipOutputStream, imagePath, "images/");
            }
            for (String pointPath : pointPaths) {
                writeFileToZip(zipOutputStream, pointPath, "points/");
            }
        }

        String filename = String.format("inspection-%d.zip", id);
        ByteArrayResource resource = new ByteArrayResource(byteArrayOutputStream.toByteArray());
        return ResponseEntity.ok()
            .header(HttpHeaders.CONTENT_DISPOSITION,
                ContentDisposition.attachment().filename(URLEncoder.encode(filename, StandardCharsets.UTF_8)).build().toString())
            .contentType(MediaType.APPLICATION_OCTET_STREAM)
            .contentLength(resource.contentLength())
            .body(resource);
    }

    public Path resolveStoragePath(String relativePath) {
        Path root = resolveStorageRoot();
        Path path = root.resolve(String.valueOf(relativePath)).normalize();
        if (!path.startsWith(root)) {
            throw new IllegalArgumentException("无效的存储路径");
        }
        return path;
    }

    public Path resolvePipeDatasetPointCloudPath(String stopId) {
        if (!PIPE_STOP_ID_PATTERN.matcher(String.valueOf(stopId)).matches()) {
            throw new IllegalArgumentException("无效的点云目录");
        }

        Path datasetRoot = resolvePipeDatasetRootPath();
        Path filePath = datasetRoot.resolve(stopId).resolve("cloud_accum.pcd").normalize();
        if (!filePath.startsWith(datasetRoot)) {
            throw new IllegalArgumentException("无效的点云路径");
        }
        return filePath;
    }

    public Path resolvePipeDatasetRootPath() {
        Path datasetRoot = Paths.get(storageProperties.getPipeDatasetRoot()).toAbsolutePath().normalize();
        log.debug("Resolved pipe dataset root: {}", datasetRoot);
        return datasetRoot;
    }

    public Path resolvePipeDatasetFittedResultPath() {
        Path datasetRoot = resolvePipeDatasetRootPath();
        Path filePath = datasetRoot.resolve("defects_global.json").normalize();
        if (!filePath.startsWith(datasetRoot)) {
            throw new IllegalArgumentException("无效的拟合结果路径");
        }
        return filePath;
    }

    public JsonNode readPipeDatasetFittedResult() throws IOException {
        Path filePath = resolvePipeDatasetFittedResultPath();
        if (!Files.exists(filePath)) {
            throw new java.nio.file.NoSuchFileException("拟合结果文件不存在: " + filePath);
        }
        return objectMapper.readTree(Files.newBufferedReader(filePath, StandardCharsets.UTF_8));
    }

    public String buildPublicUrl(String relativePath) {
        if (!StringUtils.hasText(relativePath)) {
            return "";
        }
        String prefix = String.valueOf(storageProperties.getPublicUrlPrefix()).replaceAll("/+$", "");
        return prefix + "/media/" + relativePath.replace('\\', '/');
    }

    private void writeFileToZip(ZipOutputStream zipOutputStream, String relativePath, String prefix) throws IOException {
        if (!StringUtils.hasText(relativePath)) {
            return;
        }

        Path filePath = resolveStoragePath(relativePath);
        if (!Files.exists(filePath)) {
            throw new IllegalStateException("媒体文件不存在: " + relativePath);
        }

        ZipEntry entry = new ZipEntry(prefix + filePath.getFileName());
        zipOutputStream.putNextEntry(entry);
        Files.copy(filePath, zipOutputStream);
        zipOutputStream.closeEntry();
    }

    private List<String> collectVideoPaths(InspectionRecord record) {
        LinkedHashSet<String> paths = new LinkedHashSet<>();
        if (StringUtils.hasText(record.getVideoPath())) {
            paths.add(record.getVideoPath());
        }
        paths.addAll(scanResultDirFiles(record.getResultDir(), getVideoDir(), null));
        return new ArrayList<>(paths);
    }

    private List<String> collectImagePaths(InspectionRecord record, List<InspectionAnomalyImage> imageEntities) {
        LinkedHashSet<String> paths = new LinkedHashSet<>();
        for (InspectionAnomalyImage image : imageEntities) {
            if (StringUtils.hasText(image.getImagePath())) {
                paths.add(image.getImagePath());
            }
        }
        paths.addAll(scanResultDirFiles(record.getResultDir(), getImageDir(), null));
        return new ArrayList<>(paths);
    }

    private List<String> collectPointPaths(InspectionRecord record, List<InspectionPointFile> pointEntities) {
        LinkedHashSet<String> paths = new LinkedHashSet<>();
        for (InspectionPointFile pointFile : pointEntities) {
            if (StringUtils.hasText(pointFile.getPointPath())) {
                paths.add(pointFile.getPointPath());
            }
        }
        paths.addAll(scanResultDirFiles(record.getResultDir(), getPointsDir(), path -> path.getFileName().toString().endsWith(".pcd")));
        return new ArrayList<>(paths);
    }

    private List<String> scanResultDirFiles(String resultDir, String subDir, java.util.function.Predicate<Path> predicate) {
        if (!StringUtils.hasText(resultDir) || !StringUtils.hasText(subDir)) {
            return Collections.emptyList();
        }

        Path dir = resolveStoragePath(resultDir + "/" + subDir);
        if (!Files.exists(dir) || !Files.isDirectory(dir)) {
            return Collections.emptyList();
        }

        try (Stream<Path> stream = Files.list(dir)) {
            return stream
                .filter(Files::isRegularFile)
                .filter(path -> predicate == null || predicate.test(path))
                .sorted(Comparator.comparing(path -> path.getFileName().toString()))
                .map(this::toRelativeStoragePath)
                .toList();
        } catch (IOException error) {
            log.warn("Failed to scan result directory: {}", dir, error);
            return Collections.emptyList();
        }
    }

    private List<InspectionMediaFileVO> toMediaVOs(List<String> paths, LocalDateTime fallbackTime) {
        List<InspectionMediaFileVO> result = new ArrayList<>();
        for (String path : paths) {
            InspectionMediaFileVO vo = new InspectionMediaFileVO();
            vo.setFilePath(path);
            vo.setFileName(Path.of(path).getFileName().toString());
            vo.setFileUrl(buildPublicUrl(path));
            vo.setCapturedAt(fallbackTime);
            result.add(vo);
        }
        return result;
    }

    private List<InspectionMediaFileVO> toMediaVOs(List<String> paths, Map<String, LocalDateTime> capturedAtMap) {
        List<InspectionMediaFileVO> result = new ArrayList<>();
        for (String path : paths) {
            InspectionMediaFileVO vo = new InspectionMediaFileVO();
            vo.setFilePath(path);
            vo.setFileName(Path.of(path).getFileName().toString());
            vo.setFileUrl(buildPublicUrl(path));
            vo.setCapturedAt(capturedAtMap.get(path));
            result.add(vo);
        }
        return result;
    }

    private String toRelativeStoragePath(Path filePath) {
        Path root = resolveStorageRoot();
        Path normalized = filePath.toAbsolutePath().normalize();
        if (!normalized.startsWith(root)) {
            throw new IllegalArgumentException("无效的媒体文件路径");
        }
        return root.relativize(normalized).toString().replace('\\', '/');
    }

    private Path resolveStorageRoot() {
        return Paths.get(storageProperties.getRootDir()).toAbsolutePath().normalize();
    }

    private String getVideoDir() {
        return StringUtils.hasText(storageProperties.getVideoDir()) ? storageProperties.getVideoDir().trim() : "videos";
    }

    private String getImageDir() {
        return StringUtils.hasText(storageProperties.getImageDir()) ? storageProperties.getImageDir().trim() : "images";
    }

    private String getPointsDir() {
        return StringUtils.hasText(storageProperties.getPointsDir()) ? storageProperties.getPointsDir().trim() : "points";
    }

    private InspectionHistoryItemVO toHistoryItem(InspectionRecord record) {
        InspectionHistoryItemVO vo = new InspectionHistoryItemVO();
        vo.setId(record.getId());
        vo.setMode(record.getMode());
        vo.setEnvironment(record.getEnvironment());
        vo.setOperator(record.getOperatorName());
        vo.setResult(record.getResultSummary());
        vo.setInspectionTime(record.getInspectionTime());
        vo.setCreatedAt(record.getCreatedAt());
        return vo;
    }

    private InspectionAnomalyImageVO toAnomalyVO(InspectionAnomalyImage image) {
        InspectionAnomalyImageVO vo = new InspectionAnomalyImageVO();
        vo.setId(image.getId());
        vo.setAnomalyType(image.getAnomalyType());
        vo.setRemark(image.getRemark());
        vo.setCapturedAt(image.getCapturedAt());
        vo.setImageUrl(buildPublicUrl(image.getImagePath()));
        return vo;
    }

    private LocalDateTime parseStartTime(String value) {
        if (!StringUtils.hasText(value)) {
            return null;
        }
        return LocalDate.parse(value, DateTimeFormatter.ISO_LOCAL_DATE).atStartOfDay();
    }

    private LocalDateTime parseEndTime(String value) {
        if (!StringUtils.hasText(value)) {
            return null;
        }
        return LocalDate.parse(value, DateTimeFormatter.ISO_LOCAL_DATE).atTime(LocalTime.MAX);
    }
}
