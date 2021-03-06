-- MySQL dump 10.13  Distrib 5.5.41, for debian-linux-gnu (x86_64)
--
-- Host: e906-db3.fnal.gov    Database: merged_roadset57_R004
-- ------------------------------------------------------
-- Server version	5.1.73

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `kTrack`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `kTrack` (
  `trackID` int(11) NOT NULL DEFAULT '0',
  `runID` int(11) NOT NULL DEFAULT '0',
  `spillID` int(11) DEFAULT NULL,
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
  PRIMARY KEY (`runID`,`trackID`),
  KEY `runeventID` (`runID`,`eventID`),
  KEY `spilleventID` (`spillID`,`eventID`),
  KEY `spill_track` (`spillID`,`trackID`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2015-02-24 17:45:39
