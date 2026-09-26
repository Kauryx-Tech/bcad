//! How the reader reacts to input it cannot fully interpret.
//!
//! DXF files in the wild are frequently *mostly* valid: a single entity with a
//! malformed radius, a truncated XDATA block, an undefined layer. Refusing the
//! whole file is unhelpful; accepting it silently is worse. [`RecoveryMode`]
//! makes the trade-off explicit and caller-selected.

use serde::{Deserialize, Serialize};

/// Policy applied when an entity or record cannot be interpreted.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
#[serde(rename_all = "lowercase")]
pub enum RecoveryMode {
    /// Abort the whole parse on the first problem, surfacing a `DxfError`.
    ///
    /// Use when the input must be read exactly or not at all.
    Strict,
    /// Skip what cannot be interpreted, keep the rest, and record a
    /// [`bcad_format::Diagnostic`] for each problem. This is the default.
    #[default]
    Recover,
}

impl RecoveryMode {
    /// `true` when a problem should abort the parse rather than be recorded.
    #[must_use]
    pub const fn is_strict(self) -> bool {
        matches!(self, Self::Strict)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_is_recover() {
        assert_eq!(RecoveryMode::default(), RecoveryMode::Recover);
        assert!(!RecoveryMode::default().is_strict());
    }

    #[test]
    fn strict_is_strict() {
        assert!(RecoveryMode::Strict.is_strict());
    }
}
