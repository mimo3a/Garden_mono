package com.example.garden.security;

import com.example.garden.model.AppUser;
import org.springframework.security.core.GrantedAuthority;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.security.core.userdetails.UserDetails;

import java.util.Collection;
import java.util.List;

public record AppUserPrincipal(Long id, String email, String passwordHash, String role) implements UserDetails {
    public static AppUserPrincipal from(AppUser user) {
        return new AppUserPrincipal(user.getId(), user.getEmail(), user.getPasswordHash(), user.getRole().name());
    }

    @Override public Collection<? extends GrantedAuthority> getAuthorities() { return List.of(new SimpleGrantedAuthority("ROLE_" + role)); }
    @Override public String getPassword() { return passwordHash; }
    @Override public String getUsername() { return email; }
}
