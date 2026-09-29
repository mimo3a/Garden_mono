package com.example.garden.controller;

import com.example.garden.model.Invitation;
import com.example.garden.model.Sensor;
import com.example.garden.repository.InvitationRepository;
import com.example.garden.repository.SensorRepository;
import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.server.ResponseStatusException;

import java.time.LocalDateTime;
import java.util.List;
import java.util.UUID;

@RestController
@RequestMapping("/api/admin/invitations")
public class InvitationController {
    private final InvitationRepository invitations;
    private final SensorRepository sensors;
    public InvitationController(InvitationRepository invitations, SensorRepository sensors) { this.invitations = invitations; this.sensors = sensors; }

    @PostMapping
    @ResponseStatus(HttpStatus.CREATED)
    public InvitationResponse create(@RequestBody CreateInvitationRequest request) {
        if (request.email() == null || request.email().isBlank() || request.deviceIds() == null || request.deviceIds().isEmpty()) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "Email and at least one sensor are required");
        }
        List<Integer> ids = request.deviceIds().stream().distinct().toList();
        for (Integer id : ids) {
            Sensor sensor = sensors.findByDeviceId(id).orElseThrow(() -> new ResponseStatusException(HttpStatus.BAD_REQUEST, "Unknown sensor " + id));
            if (sensor.getOwner() != null) throw new ResponseStatusException(HttpStatus.CONFLICT, "Sensor " + id + " is already assigned");
        }
        Invitation invitation = new Invitation();
        invitation.setToken(UUID.randomUUID().toString());
        invitation.setEmail(request.email().trim().toLowerCase());
        invitation.setDeviceIds(ids);
        invitation.setExpiresAt(LocalDateTime.now().plusDays(7));
        invitation = invitations.save(invitation);
        return InvitationResponse.from(invitation);
    }

    public record CreateInvitationRequest(String email, List<Integer> deviceIds) {}
    public record InvitationResponse(String token, String email, List<Integer> deviceIds, LocalDateTime expiresAt) {
        static InvitationResponse from(Invitation invitation) { return new InvitationResponse(invitation.getToken(), invitation.getEmail(), invitation.getDeviceIds(), invitation.getExpiresAt()); }
    }
}
