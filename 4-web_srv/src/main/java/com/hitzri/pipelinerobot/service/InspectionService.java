package com.hitzri.pipelinerobot.service;

import com.baomidou.mybatisplus.core.conditions.query.LambdaQueryWrapper;
import com.baomidou.mybatisplus.extension.plugins.pagination.Page;
import com.hitzri.pipelinerobot.config.StorageProperties;
import com.hitzri.pipelinerobot.entity.InspectionAnomalyImage;
import com.hitzri.pipelinerobot.entity.InspectionRecord;
import com.hitzri.pipelinerobot.mapper.InspectionAnomalyImageMapper;
import com.hitzri.pipelinerobot.mapper.InspectionRecordMapper;
import com.hitzri.pipelinerobot.vo.InspectionAnomalyImageVO;
import com.hitzri.pipelinerobot.vo.InspectionDetailVO;
import com.hitzri.pipelinerobot.vo.InspectionHistoryItemVO;
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
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;
import org.springframework.core.io.ByteArrayResource;
import org.springframework.http.ContentDisposition;
import org.springframework.http.HttpHeaders;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.stereotype.Service;
import org.springframework.util.StringUtils;

@Service
public class InspectionService {

    private final InspectionRecordMapper inspectionRecordMapper;
    private final InspectionAnomalyImageMapper anomalyImageMapper;
    private final StorageProperties storageProperties;

    public InspectionService(
        InspectionRecordMapper inspectionRecordMapper,
        InspectionAnomalyImageMapper anomalyImageMapper,
        StorageProperties storageProperties
    ) {
        this.inspectionRecordMapper = inspectionRecordMapper;
        this.anomalyImageMapper = anomalyImageMapper;
        this.storageProperties = storageProperties;
    }

    public PageResponse<InspectionHistoryItemVO> getHistory(
        String mode,
        String environment,
        String startDate,
        String endDate,
        long page,
        long pageSize
    ) {
        LambdaQueryWrapper<InspectionRecord> wrapper = new LambdaQueryWrapper<>();
        wrapper.like(StringUtils.hasText(mode), InspectionRecord::getMode, mode)
            .like(StringUtils.hasText(environment), InspectionRecord::getEnvironment, environment)
            .ge(StringUtils.hasText(startDate), InspectionRecord::getInspectionTime, parseStartTime(startDate))
            .le(StringUtils.hasText(endDate), InspectionRecord::getInspectionTime, parseEndTime(endDate))
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

        LambdaQueryWrapper<InspectionAnomalyImage> wrapper = new LambdaQueryWrapper<>();
        wrapper.eq(InspectionAnomalyImage::getInspectionId, id).orderByAsc(InspectionAnomalyImage::getCapturedAt);
        List<InspectionAnomalyImageVO> anomalies = anomalyImageMapper.selectList(wrapper).stream().map(this::toAnomalyVO).toList();

        InspectionDetailVO vo = new InspectionDetailVO();
        vo.setId(record.getId());
        vo.setMode(record.getMode());
        vo.setEnvironment(record.getEnvironment());
        vo.setOperator(record.getOperatorName());
        vo.setResult(record.getResultSummary());
        vo.setInspectionTime(record.getInspectionTime());
        vo.setCreatedAt(record.getCreatedAt());
        vo.setVideoUrl(buildPublicUrl(record.getVideoPath()));
        vo.setAnomalies(anomalies);
        return vo;
    }

    public ResponseEntity<ByteArrayResource> exportRecord(Long id) throws IOException {
        InspectionRecord record = inspectionRecordMapper.selectById(id);
        if (record == null) {
            throw new IllegalArgumentException("巡检记录不存在");
        }

        LambdaQueryWrapper<InspectionAnomalyImage> wrapper = new LambdaQueryWrapper<>();
        wrapper.eq(InspectionAnomalyImage::getInspectionId, id).orderByAsc(InspectionAnomalyImage::getCapturedAt);
        List<InspectionAnomalyImage> anomalies = anomalyImageMapper.selectList(wrapper);

        ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
        try (ZipOutputStream zipOutputStream = new ZipOutputStream(byteArrayOutputStream)) {
            writeFileToZip(zipOutputStream, record.getVideoPath(), "videos/");
            for (InspectionAnomalyImage image : anomalies) {
                writeFileToZip(zipOutputStream, image.getImagePath(), "images/");
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
        return Paths.get(storageProperties.getRootDir()).resolve(relativePath).normalize();
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

    private String buildPublicUrl(String relativePath) {
        if (!StringUtils.hasText(relativePath)) {
            return "";
        }
        return storageProperties.getPublicUrlPrefix() + "/" + relativePath.replace('\\', '/');
    }

    private LocalDateTime parseStartTime(String value) {
        return LocalDate.parse(value, DateTimeFormatter.ISO_LOCAL_DATE).atStartOfDay();
    }

    private LocalDateTime parseEndTime(String value) {
        return LocalDate.parse(value, DateTimeFormatter.ISO_LOCAL_DATE).atTime(LocalTime.MAX);
    }
}
