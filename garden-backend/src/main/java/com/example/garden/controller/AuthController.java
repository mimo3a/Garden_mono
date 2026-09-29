package com.example.garden.controller;

import com.example.garden.model.AppUser;
import com.example.garden.model.Invitation;
import com.example.garden.model.Role;
import com.example.garden.model.Sensor;
import com.example.garden.repository.AppUserRepository;
import com.example.garden.repository.InvitationRepository;
import com.example.garden.repository.SensorRepository;
import com.example.garden.security.AppUserPrincipal;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpSession;
import org.springframework.http.HttpStatus;
import org.springframework.security.authentication.AuthenticationManager;
import org.springframework.security.authentication.UsernamePasswordAuthenticationToken;
import org.springframework.security.core.Authentication;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.security.core.context.SecurityContextImpl;
import org.springframework.security.core.annotation.AuthenticationPrincipal;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.security.web.context.HttpSessionSecurityContextRepository;
import org.springframework.security.web.csrf.CsrfToken;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.server.ResponseStatusException;

import java.time.LocalDateTime;

@RestController
@RequestMapping("/api/auth")
public class AuthController {
    private final AuthenticationManager authenticationManager;
    private final AppUserRepository users;
    private final InvitationRepository invitations;
    private final SensorRepository sensors;
    private final PasswordEncoder passwordEncoder;

    public AuthController(AuthenticationManager authenticationManager, AppUserRepository users, InvitationRepository invitations,
                          SensorRepository sensors, PasswordEncoder passwordEncoder) {
        this.authenticationManager = authenticationManager;
        this.users = users;
        this.invitations = invitations;
        this.sensors = sensors;
        this.passwordEncoder = passwordEncoder;
    }

    @GetMapping("/csrf")
    public CsrfToken csrf(CsrfToken token) { return token; }

    @PostMapping("/login")
    public UserResponse login(@RequestBody LoginRequest request, HttpServletRequest httpRequest) {
        Authentication authentication = authenticationManager.authenticate(
                UsernamePasswordAuthenticationToken.unauthenticated(request.email().trim().toLowerCase(), request.password()));
        httpRequest.changeSessionId();
        SecurityContextImpl context = new SecurityContextImpl(authentication);
        SecurityContextHolder.setContext(context);
        httpRequest.getSession(true).setAttribute(HttpSessionSecurityContextRepository.SPRING_SECURITY_CONTEXT_KEY, context);
        return UserResponse.from((AppUserPrincipal) authentication.getPrincipal());
    }

    @GetMapping("/me")
    public UserResponse me(@AuthenticationPrincipal AppUserPrincipal user) { return UserResponse.from(user); }

    @PostMapping("/logout")
    @ResponseStatus(HttpStatus.NO_CONTENT)
    public void logout(HttpServletRequest request) {
        HttpSession session = request.getSession(false);
        if (session != null) session.invalidate();
        SecurityContextHolder.clearContext();
    }

    @PostMapping("/accept-invitation")
    public UserResponse acceptInvitation(@RequestBody AcceptInvitationRequest request) {
        Invitation invitation = invitations.findByToken(request.token()).orElseThrow(() -> new ResponseStatusException(HttpStatus.NOT_FOUND, "Invitation not found"));
        if (invitation.getAcceptedAt() != null || invitation.getExpiresAt().isBefore(LocalDateTime.now())) {
            throw new ResponseStatusException(HttpStatus.GONE, "Invitation is expired or already used");
        }
        if (request.password() == null || request.password().length() < 12) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "Password must contain at least 12 characters");
        }
        if (users.findByEmailIgnoreCase(invitation.getEmail()).isPresent()) {
            throw new ResponseStatusException(HttpStatus.CONFLICT, "An account with this email already exists");
        }
        AppUser user = new AppUser();
        user.setEmail(invitation.getEmail().toLowerCase());
        user.setPasswordHash(passwordEncoder.encode(request.password()));
        user.setRole(Role.USER);
        user = users.save(user);
        for (Integer deviceId : invitation.getDeviceIds()) {
            Sensor sensor = sensors.findByDeviceId(deviceId).orElseThrow(() -> new ResponseStatusException(HttpStatus.BAD_REQUEST, "Unknown sensor " + deviceId));
            if (sensor.getOwner() != null) throw new ResponseStatusException(HttpStatus.CONFLICT, "Sensor " + deviceId + " is already assigned");
            sensor.setOwner(user);
            sensors.save(sensor);
        }
        invitation.setAcceptedAt(LocalDateTime.now());
        invitations.save(invitation);
        return UserResponse.from(AppUserPrincipal.from(user));
    }

    public record LoginRequest(String email, String password) {}
    public record AcceptInvitationRequest(String token, String password) {}
    public record UserResponse(Long id, String email, String role) {
        static UserResponse from(AppUserPrincipal user) { return new UserResponse(user.id(), user.email(), user.role()); }
    }
}
