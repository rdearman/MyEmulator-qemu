Distribution-owned rootfs overlay files live here. Package payloads install
first; this overlay contains only shared configuration that is not owned by a
package. Profile-specific configuration stays under `profiles/`.
