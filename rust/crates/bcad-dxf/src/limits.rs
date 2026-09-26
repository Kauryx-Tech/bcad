//! Resource limits applied while reading untrusted DXF input.
//!
//! A DXF file is attacker-controlled input as far as this crate is concerned:
//! a drawing declares its own polyline vertex count, layer count and XDATA
//! size, and a hostile file can claim far more than it delivers. Limits are
//! therefore checked *against the source declarations* while reading, not only
//! against what ends up in memory.

use serde::{Deserialize, Serialize};

use crate::error::{DxfError, DxfResult};

/// Bounds applied to a single parse.
///
/// Every limit is a hard ceiling: exceeding one aborts the parse with
/// [`DxfError::ResourceLimit`]. There is no clamping, because a silently
/// clamped geometry is a corrupt geometry.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ParseLimits {
    /// Largest accepted input, in bytes.
    pub max_file_size: usize,
    /// Largest accepted single group value, in bytes.
    pub max_line_length: usize,
    /// Largest accepted number of groups in the whole file.
    pub max_tokens: usize,
    /// Largest accepted number of entities.
    pub max_entities: usize,
    /// Largest accepted number of vertices in one polyline.
    pub max_polyline_vertices: usize,
    /// Largest accepted number of layers.
    pub max_layers: usize,
    /// Largest accepted XDATA payload for one entity, in bytes.
    pub max_xdata_size: usize,
    /// Largest accepted `SECTION`/`TABLE` nesting depth.
    pub max_nesting_depth: usize,
}

impl Default for ParseLimits {
    fn default() -> Self {
        Self {
            max_file_size: 64 * 1024 * 1024,
            max_line_length: 1024 * 1024,
            max_tokens: 8_000_000,
            max_entities: 1_000_000,
            max_polyline_vertices: 1_000_000,
            max_layers: 100_000,
            max_xdata_size: 1024 * 1024,
            max_nesting_depth: 32,
        }
    }
}

impl ParseLimits {
    /// Tighter bounds for input that is not trusted at all.
    #[must_use]
    pub const fn strict() -> Self {
        Self {
            max_file_size: 8 * 1024 * 1024,
            max_line_length: 64 * 1024,
            max_tokens: 500_000,
            max_entities: 50_000,
            max_polyline_vertices: 50_000,
            max_layers: 1_000,
            max_xdata_size: 64 * 1024,
            max_nesting_depth: 8,
        }
    }

    /// Checks the total input size.
    /// # Errors
    ///
    /// Returns [`DxfError::ResourceLimit`] when the input is larger than `max_file_size`.
    pub fn check_file_size(&self, size: usize) -> DxfResult<()> {
        if size > self.max_file_size {
            return Err(DxfError::resource_limit(format!(
                "input of {size} bytes exceeds the {} byte limit",
                self.max_file_size
            )));
        }
        Ok(())
    }

    /// Checks one group value length.
    /// # Errors
    ///
    /// Returns [`DxfError::ResourceLimit`] when a group value is longer than `max_line_length`.
    pub fn check_line_length(&self, len: usize) -> DxfResult<()> {
        if len > self.max_line_length {
            return Err(DxfError::resource_limit(format!(
                "group value of {len} bytes exceeds the {} byte limit",
                self.max_line_length
            )));
        }
        Ok(())
    }

    /// Checks the total group count.
    /// # Errors
    ///
    /// Returns [`DxfError::ResourceLimit`] when the group count exceeds `max_tokens`.
    pub fn check_token_count(&self, count: usize) -> DxfResult<()> {
        if count > self.max_tokens {
            return Err(DxfError::resource_limit(format!(
                "{count} groups exceed the {} group limit",
                self.max_tokens
            )));
        }
        Ok(())
    }

    /// Checks the entity count.
    /// # Errors
    ///
    /// Returns [`DxfError::ResourceLimit`] when the entity count exceeds `max_entities`.
    pub fn check_entity_count(&self, count: usize) -> DxfResult<()> {
        if count > self.max_entities {
            return Err(DxfError::resource_limit(format!(
                "{count} entities exceed the {} entity limit",
                self.max_entities
            )));
        }
        Ok(())
    }

    /// Checks the vertex count of a single polyline.
    /// # Errors
    ///
    /// Returns [`DxfError::ResourceLimit`] when a polyline has more than `max_polyline_vertices` vertices.
    pub fn check_polyline_vertices(&self, count: usize) -> DxfResult<()> {
        if count > self.max_polyline_vertices {
            return Err(DxfError::resource_limit(format!(
                "{count} polyline vertices exceed the {} vertex limit",
                self.max_polyline_vertices
            )));
        }
        Ok(())
    }

    /// Checks the layer count.
    /// # Errors
    ///
    /// Returns [`DxfError::ResourceLimit`] when the layer count exceeds `max_layers`.
    pub fn check_layer_count(&self, count: usize) -> DxfResult<()> {
        if count > self.max_layers {
            return Err(DxfError::resource_limit(format!(
                "{count} layers exceed the {} layer limit",
                self.max_layers
            )));
        }
        Ok(())
    }

    /// Checks the XDATA payload accumulated for one entity.
    /// # Errors
    ///
    /// Returns [`DxfError::ResourceLimit`] when one entity's XDATA exceeds `max_xdata_size`.
    pub fn check_xdata_size(&self, size: usize) -> DxfResult<()> {
        if size > self.max_xdata_size {
            return Err(DxfError::resource_limit(format!(
                "{size} bytes of XDATA exceed the {} byte limit",
                self.max_xdata_size
            )));
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn accepts_input_within_limits() {
        let limits = ParseLimits::default();
        assert!(limits.check_file_size(1024).is_ok());
        assert!(limits.check_entity_count(10).is_ok());
    }

    #[test]
    fn rejects_input_over_each_limit() {
        let limits = ParseLimits::default();
        assert!(limits.check_file_size(limits.max_file_size + 1).is_err());
        assert!(limits
            .check_line_length(limits.max_line_length + 1)
            .is_err());
        assert!(limits.check_token_count(limits.max_tokens + 1).is_err());
        assert!(limits.check_entity_count(limits.max_entities + 1).is_err());
        assert!(limits
            .check_polyline_vertices(limits.max_polyline_vertices + 1)
            .is_err());
        assert!(limits.check_layer_count(limits.max_layers + 1).is_err());
        assert!(limits.check_xdata_size(limits.max_xdata_size + 1).is_err());
    }

    #[test]
    fn strict_limits_are_tighter_than_default() {
        let strict = ParseLimits::strict();
        let default = ParseLimits::default();
        assert!(strict.max_file_size < default.max_file_size);
        assert!(strict.max_entities < default.max_entities);
        assert!(strict.max_tokens < default.max_tokens);
    }

    #[test]
    fn error_names_the_limit() {
        let limits = ParseLimits::strict();
        let err = limits
            .check_entity_count(limits.max_entities + 1)
            .expect_err("must fail");
        assert!(err.to_string().contains("entity limit"), "{err}");
    }
}
