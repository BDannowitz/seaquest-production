CREATE TABLE `kTrackMix` (
  `trackID` int(11) NOT NULL DEFAULT '0',
  `runID` int(11) NOT NULL DEFAULT '0',
  `eventID` int(11) NOT NULL DEFAULT '0',
  `charge` int(11) DEFAULT NULL,
  `roadID` int(11) DEFAULT NULL,
  `numHits` int(11) DEFAULT NULL,
  `numHitsSt1` int(11) DEFAULT NULL,
  `numHitsSt2` int(11) DEFAULT NULL,
  `numHitsSt3` int(11) DEFAULT NULL,
  `numHitsSt4H` int(11) DEFAULT NULL,
  `numHitsSt4V` int(11) DEFAULT NULL,
  `chisq` double DEFAULT NULL,
  `x0` double DEFAULT NULL,
  `y0` double DEFAULT NULL,
  `z0` double DEFAULT NULL,
  `x_target` double DEFAULT NULL,
  `y_target` double DEFAULT NULL,
  `z_target` double DEFAULT NULL,
  `x_dump` double DEFAULT NULL,
  `y_dump` double DEFAULT NULL,
  `z_dump` double DEFAULT NULL,
  `px0` double DEFAULT NULL,
  `py0` double DEFAULT NULL,
  `pz0` double DEFAULT NULL,
  `x1` double DEFAULT NULL,
  `y1` double DEFAULT NULL,
  `z1` double DEFAULT NULL,
  `px1` double DEFAULT NULL,
  `py1` double DEFAULT NULL,
  `pz1` double DEFAULT NULL,
  `x3` double DEFAULT NULL,
  `y3` double DEFAULT NULL,
  `z3` double DEFAULT NULL,
  `px3` double DEFAULT NULL,
  `py3` double DEFAULT NULL,
  `pz3` double DEFAULT NULL,
  `tx_PT` double DEFAULT NULL,
  `ty_PT` double DEFAULT NULL,
  PRIMARY KEY (`runID`,`eventID`,`trackID`),
  KEY `eventID` (`eventID`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

