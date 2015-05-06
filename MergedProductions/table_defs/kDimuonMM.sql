CREATE TABLE `kDimuonMM` (
  `dimuonID` int(11) NOT NULL DEFAULT '0',
  `runID` int(11) NOT NULL DEFAULT '0',
  `spillID` int(11) DEFAULT NULL,
  `eventID` int(11) NOT NULL DEFAULT '0',
  `posTrackID` int(11) DEFAULT NULL,
  `negTrackID` int(11) DEFAULT NULL,
  `dx` double DEFAULT NULL,
  `dy` double DEFAULT NULL,
  `dz` double DEFAULT NULL,
  `dpx` double DEFAULT NULL,
  `dpy` double DEFAULT NULL,
  `dpz` double DEFAULT NULL,
  `mass` double DEFAULT NULL,
  `xF` double DEFAULT NULL,
  `xB` double DEFAULT NULL,
  `xT` double DEFAULT NULL,
  `trackSeparation` double DEFAULT NULL,
  `chisq_dimuon` double DEFAULT NULL,
  `px1` double DEFAULT NULL,
  `py1` double DEFAULT NULL,
  `pz1` double DEFAULT NULL,
  `px2` double DEFAULT NULL,
  `py2` double DEFAULT NULL,
  `pz2` double DEFAULT NULL,
  `isValid` int(11) DEFAULT NULL,
  `isTarget` int(11) DEFAULT NULL,
  `isDump` int(11) DEFAULT NULL,
  PRIMARY KEY (`runID`,`eventID`,`dimuonID`),
  KEY `eventID` (`eventID`),
  KEY `spillID` (`spillID`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

